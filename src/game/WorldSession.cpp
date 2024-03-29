/*
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * Copyright (C) 2008-2010 Trinity <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/** \file
    \ingroup u2w
*/

#include "WorldSocket.h"                                    // must be first to make ACE happy with ACE includes in it
#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "Vehicle.h"
#include "ObjectMgr.h"
#include "Group.h"
#include "Guild.h"
#include "World.h"
#include "ObjectAccessor.h"
#include "BattleGroundMgr.h"
#include "OutdoorPvPMgr.h"
#include "MapManager.h"
#include "SocialMgr.h"
#include "zlib/zlib.h"
#include "ScriptMgr.h"

/// WorldSession constructor
WorldSession::WorldSession(uint32 id, WorldSocket *sock, AccountTypes sec, uint8 expansion, time_t mute_time, LocaleConstant locale) :
LookingForGroup_auto_join(false), LookingForGroup_auto_add(false), m_muteTime(mute_time),
_player(NULL), m_Socket(sock),_security(sec), _accountId(id), m_expansion(expansion),
m_sessionDbcLocale(sWorld.GetAvailableDbcLocale(locale)), m_sessionDbLocaleIndex(objmgr.GetIndexForLocale(locale)),
_logoutTime(0), m_inQueue(false), m_playerLoading(false), m_playerLogout(false), m_playerRecentlyLogout(false), m_playerSave(false),
m_latency(0), m_TutorialsChanged(false)
{
    if (sock)
    {
        m_Address = sock->GetRemoteAddress ();
        sock->AddReference ();
        loginDatabase.PExecute("UPDATE account SET online = 1 WHERE id = %u;", GetAccountId());
    }
}

/// WorldSession destructor
WorldSession::~WorldSession()
{
    ///- unload player if not unloaded
    if (_player)
        LogoutPlayer (true);

    /// - If have unclosed socket, close it
    if (m_Socket)
    {
        m_Socket->CloseSocket ();
        m_Socket->RemoveReference ();
        m_Socket = NULL;
    }

    ///- empty incoming packet queue
    WorldPacket* packet;
    while (_recvQueue.next(packet))
        delete packet;

    loginDatabase.PExecute("UPDATE account SET online = 0 WHERE id = %u;", GetAccountId());
    CharacterDatabase.PExecute("UPDATE characters SET online = 0 WHERE account = %u;", GetAccountId());
}

void WorldSession::SizeError(WorldPacket const& packet, uint32 size) const
{
    sLog.outError("Client (account %u) send packet %s (%u) with size " SIZEFMTD " but expected %u (attempt crash server?), skipped",
        GetAccountId(),LookupOpcodeName(packet.GetOpcode()),packet.GetOpcode(),packet.size(),size);
}

/// Get the player name
char const* WorldSession::GetPlayerName() const
{
    return GetPlayer() ? GetPlayer()->GetName() : "<none>";
}

/// Send a packet to the client
void WorldSession::SendPacket(WorldPacket const* packet)
{
    if (!m_Socket)
        return;

    #ifdef TRINITY_DEBUG

    // Code for network use statistic
    static uint64 sendPacketCount = 0;
    static uint64 sendPacketBytes = 0;

    static time_t firstTime = time(NULL);
    static time_t lastTime = firstTime;                     // next 60 secs start time

    static uint64 sendLastPacketCount = 0;
    static uint64 sendLastPacketBytes = 0;

    time_t cur_time = time(NULL);

    if ((cur_time - lastTime) < 60)
    {
        sendPacketCount+=1;
        sendPacketBytes+=packet->size();

        sendLastPacketCount+=1;
        sendLastPacketBytes+=packet->size();
    }
    else
    {
        uint64 minTime = uint64(cur_time - lastTime);
        uint64 fullTime = uint64(lastTime - firstTime);
        sLog.outDetail("Send all time packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f time: %u",sendPacketCount,sendPacketBytes,float(sendPacketCount)/fullTime,float(sendPacketBytes)/fullTime,uint32(fullTime));
        sLog.outDetail("Send last min packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f",sendLastPacketCount,sendLastPacketBytes,float(sendLastPacketCount)/minTime,float(sendLastPacketBytes)/minTime);

        lastTime = cur_time;
        sendLastPacketCount = 1;
        sendLastPacketBytes = packet->wpos();               // wpos is real written size
    }

    #endif                                                  // !TRINITY_DEBUG

    if (m_Socket->SendPacket (*packet) == -1)
        m_Socket->CloseSocket ();
}

/// Add an incoming packet to the queue
void WorldSession::QueuePacket(WorldPacket* new_packet)
{
    _recvQueue.add(new_packet);
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnexpectedOpcode(WorldPacket* packet, const char *reason)
{
    sLog.outError("SESSION: received unexpected opcode %s (0x%.4X) %s",
        LookupOpcodeName(packet->GetOpcode()),
        packet->GetOpcode(),
        reason);
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnprocessedTail(WorldPacket *packet)
{
    sLog.outError("SESSION: opcode %s (0x%.4X) have unprocessed tail data (read stop at %u from %u)",
        LookupOpcodeName(packet->GetOpcode()),
        packet->GetOpcode(),
        packet->rpos(),packet->wpos());

        packet->print_storage();
}

/// Update the WorldSession (triggered by World update)
bool WorldSession::Update(uint32 /*diff*/)
{
    ///- Retrieve packets from the receive queue and call the appropriate handlers
    /// not proccess packets if socket already closed
    WorldPacket* packet;
    while (m_Socket && !m_Socket->IsClosed() && _recvQueue.next(packet))
    {
        /*#if 1
        sLog.outError("MOEP: %s (0x%.4X)",
                        LookupOpcodeName(packet->GetOpcode()),
                        packet->GetOpcode());
        #endif*/

        if (packet->GetOpcode() >= NUM_MSG_TYPES)
        {
            sLog.outError("SESSION: received non-existed opcode %s (0x%.4X)",
                LookupOpcodeName(packet->GetOpcode()),
                packet->GetOpcode());
        }
        else
        {
            OpcodeHandler& opHandle = opcodeTable[packet->GetOpcode()];
            try
            {
                switch (opHandle.status)
                {
                    case STATUS_LOGGEDIN:
                        if (!_player)
                        {
                            // skip STATUS_LOGGEDIN opcode unexpected errors if player logout sometime ago - this can be network lag delayed packets
                            if (!m_playerRecentlyLogout)
                                LogUnexpectedOpcode(packet, "the player has not logged in yet");
                        }
                        else if (_player->IsInWorld())
                        {
                            (this->*opHandle.handler)(*packet);
                            if (sLog.IsOutDebug() && packet->rpos() < packet->wpos())
                                LogUnprocessedTail(packet);
                        }
                        // lag can cause STATUS_LOGGEDIN opcodes to arrive after the player started a transfer
                        break;
                    case STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT:
                        if (!_player && !m_playerRecentlyLogout)
                        {
                            LogUnexpectedOpcode(packet, "the player has not logged in yet and not recently logout");
                        }
                        else
                        {
                            // not expected _player or must checked in packet hanlder
                            (this->*opHandle.handler)(*packet);
                            if (sLog.IsOutDebug() && packet->rpos() < packet->wpos())
                                LogUnprocessedTail(packet);
                        }
                        break;
                    case STATUS_TRANSFER:
                        if (!_player)
                            LogUnexpectedOpcode(packet, "the player has not logged in yet");
                        else if (_player->IsInWorld())
                            LogUnexpectedOpcode(packet, "the player is still in world");
                        else
                        {
                            (this->*opHandle.handler)(*packet);
                            if (sLog.IsOutDebug() && packet->rpos() < packet->wpos())
                                LogUnprocessedTail(packet);
                        }
                        break;
                    case STATUS_AUTHED:
                        // prevent cheating with skip queue wait
                        if (m_inQueue)
                        {
                            LogUnexpectedOpcode(packet, "the player not pass queue yet");
                            break;
                        }

                        // single from authed time opcodes send in to after logout time
                        // and before other STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT opcodes.
                        if (packet->GetOpcode() != CMSG_SET_ACTIVE_VOICE_CHANNEL)
                            m_playerRecentlyLogout = false;

                        (this->*opHandle.handler)(*packet);
                        if (sLog.IsOutDebug() && packet->rpos() < packet->wpos())
                            LogUnprocessedTail(packet);
                        break;
                    case STATUS_NEVER:
                        break;
                        sLog.outError("SESSION: received not allowed opcode %s (0x%.4X)",
                            LookupOpcodeName(packet->GetOpcode()),
                            packet->GetOpcode());
                        break;
                }
            }
            catch(ByteBufferException &)
            {
                sLog.outError("WorldSession::Update ByteBufferException occured while parsing a packet (opcode: %u) from client %s, accountid=%i. Skipped packet.",
                        packet->GetOpcode(), GetRemoteAddress().c_str(), GetAccountId());
                if (sLog.IsOutDebug())
                {
                    sLog.outDebug("Dumping error causing packet:");
                    packet->hexlike();
                }
            }
        }

        delete packet;
    }

    ///- Cleanup socket pointer if need
    if (m_Socket && m_Socket->IsClosed ())
    {
        m_Socket->RemoveReference ();
        m_Socket = NULL;
    }

    ///- If necessary, log the player out
    time_t currTime = time(NULL);
    if (!m_Socket || (ShouldLogOut(currTime) && !m_playerLoading))
        LogoutPlayer(true);

    if (!m_Socket)
        return false;                                       //Will remove this session from the world session map

    return true;
}

/// %Log the player out
void WorldSession::LogoutPlayer(bool Save)
{
    // finish pending transfers before starting the logout
    while (_player && _player->IsBeingTeleportedFar())
        HandleMoveWorldportAckOpcode();

    m_playerLogout = true;
    m_playerSave = Save;

    if (_player)
    {
        if (uint64 lguid = GetPlayer()->GetLootGUID())
            DoLootRelease(lguid);

        ///- If the player just died before logging out, make him appear as a ghost
        //FIXME: logout must be delayed in case lost connection with client in time of combat
        if (_player->GetDeathTimer())
        {
            _player->getHostileRefManager().deleteReferences();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }
        else if (!_player->getAttackers().empty())
        {
            _player->CombatStop();
            _player->getHostileRefManager().setOnlineOfflineState(false);
            _player->RemoveAllAurasOnDeath();

            // build set of player who attack _player or who have pet attacking of _player
            std::set<Player*> aset;
            for (Unit::AttackerSet::const_iterator itr = _player->getAttackers().begin(); itr != _player->getAttackers().end(); ++itr)
            {
                Unit* owner = (*itr)->GetOwner();           // including player controlled case
                if (owner)
                {
                    if (owner->GetTypeId() == TYPEID_PLAYER)
              aset.insert(owner->ToPlayer());
                }
                else
                if ((*itr)->GetTypeId() == TYPEID_PLAYER)
                    aset.insert((Player*)(*itr));
            }

            _player->SetPvPDeath(!aset.empty());
            _player->KillPlayer();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();

            // give honor to all attackers from set like group case
            for (std::set<Player*>::const_iterator itr = aset.begin(); itr != aset.end(); ++itr)
                (*itr)->RewardHonor(_player,aset.size());

            // give bg rewards and update counters like kill by first from attackers
            // this can't be called for all attackers.
            if (!aset.empty())
                if (BattleGround *bg = _player->GetBattleGround())
                    bg->HandleKillPlayer(_player,*aset.begin());
        }
        else if (_player->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            // this will kill character by SPELL_AURA_SPIRIT_OF_REDEMPTION
            _player->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
            //_player->SetDeathPvP(*); set at SPELL_AURA_SPIRIT_OF_REDEMPTION apply time
            _player->KillPlayer();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }
        //drop a flag if player is carrying it
        if (BattleGround *bg = _player->GetBattleGround())
            bg->EventPlayerLoggedOut(_player);

        ///- Teleport to home if the player is in an invalid instance
        if (!_player->m_InstanceValid && !_player->isGameMaster())
            _player->TeleportTo(_player->m_homebindMapId, _player->m_homebindX, _player->m_homebindY, _player->m_homebindZ, _player->GetOrientation());

        sOutdoorPvPMgr.HandlePlayerLeaveZone(_player,_player->GetZoneId());

        for (int i=0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            if (BattleGroundQueueTypeId bgQueueTypeId = _player->GetBattleGroundQueueTypeId(i))
            {
                _player->RemoveBattleGroundQueueId(bgQueueTypeId);
                sBattleGroundMgr.m_BattleGroundQueues[ bgQueueTypeId ].RemovePlayer(_player->GetGUID(), true);
            }
        }

        // Repop at GraveYard or other player far teleport will prevent saving player because of not present map
        // Teleport player immediately for correct player save
        while (_player->IsBeingTeleportedFar())
            HandleMoveWorldportAckOpcode();

        ///- If the player is in a guild, update the guild roster and broadcast a logout message to other guild members
        Guild *guild = objmgr.GetGuildById(_player->GetGuildId());
        if (guild)
        {
            guild->SetMemberStats(_player->GetGUID());
            guild->UpdateLogoutTime(_player->GetGUID());

            guild->BroadcastEvent(GE_SIGNED_OFF, _player->GetGUID(), 1, _player->GetName(), "", "");
        }

        ///- Remove pet
        _player->RemovePet(NULL,PET_SAVE_AS_CURRENT, true);

        ///- empty buyback items and save the player in the database
        // some save parts only correctly work in case player present in map/player_lists (pets, etc)
        if (Save)
        {
            uint32 eslot;
            for (int j = BUYBACK_SLOT_START; j < BUYBACK_SLOT_END; ++j)
            {
                eslot = j - BUYBACK_SLOT_START;
                _player->SetUInt64Value(PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + (eslot * 2), 0);
                _player->SetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + eslot, 0);
                _player->SetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + eslot, 0);
            }
            _player->SaveToDB();
        }

        ///- Leave all channels before player delete...
        _player->CleanupChannels();

        ///- If the player is in a group (or invited), remove him. If the group if then only 1 person, disband the group.
        _player->UninviteFromGroup();

        // remove player from the group if he is:
        // a) in group; b) not in raid group; c) logging out normally (not being kicked or disconnected)
        if (_player->GetGroup() && !_player->GetGroup()->isRaidGroup() && m_Socket)
            _player->RemoveFromGroup();

        ///- Send update to group
        if (_player->GetGroup())
            _player->GetGroup()->SendUpdate();

        ///- Broadcast a logout message to the player's friends
        sSocialMgr.SendFriendStatus(_player, FRIEND_OFFLINE, _player->GetGUIDLow(), true);
        sSocialMgr.RemovePlayerSocial (_player->GetGUIDLow ());

        ///- Remove the player from the world
        // the player may not be in the world when logging out
        // e.g if he got disconnected during a transfer to another map
        // calls to GetMap in this case may cause crashes
        _player->CleanupsBeforeDelete();
        Map* _map = _player->GetMap();
        _map->Remove(_player, true);
        SetPlayer(NULL);                                    // deleted in Remove call

        ///- Send the 'logout complete' packet to the client
        WorldPacket data(SMSG_LOGOUT_COMPLETE, 0);
        SendPacket(&data);

        ///- Since each account can only have one online character at any given time, ensure all characters for active account are marked as offline
        //No SQL injection as AccountId is uint32
        CharacterDatabase.PExecute("UPDATE characters SET online = 0 WHERE account = '%u'",
            GetAccountId());
        sLog.outDebug("SESSION: Sent SMSG_LOGOUT_COMPLETE Message");
    }

    //Hook for OnLogout Event
    sScriptMgr.OnLogout(_player);

    m_playerLogout = false;
    m_playerSave = false;
    m_playerRecentlyLogout = true;
    LogoutRequest(0);
}

/// Kick a player out of the World
void WorldSession::KickPlayer()
{
    if (m_Socket)
        m_Socket->CloseSocket ();
}

void WorldSession::SendNotification(const char *format,...)
{
    if (format)
    {
        va_list ap;
        char szStr [1024];
        szStr[0] = '\0';
        va_start(ap, format);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr)+1));
        data << szStr;
        SendPacket(&data);
    }
}

void WorldSession::SendNotification(int32 string_id,...)
{
    char const* format = GetTrinityString(string_id);
    if (format)
    {
        va_list ap;
        char szStr [1024];
        szStr[0] = '\0';
        va_start(ap, string_id);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr)+1));
        data << szStr;
        SendPacket(&data);
    }
}

const char * WorldSession::GetTrinityString(int32 entry) const
{
    return objmgr.GetTrinityString(entry,GetSessionDbLocaleIndex());
}

void WorldSession::Handle_NULL(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received unhandled opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

void WorldSession::Handle_EarlyProccess(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received opcode %s (0x%.4X) that must be processed in WorldSocket::OnRead",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

void WorldSession::Handle_ServerSide(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received server-side opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

void WorldSession::Handle_Deprecated(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received deprecated opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

void WorldSession::SendAuthWaitQue(uint32 position)
{
    if (position == 0)
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_OK);
        SendPacket(&packet);
    }
    else
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 5);
        packet << uint8(AUTH_WAIT_QUEUE);
        packet << uint32 (position);
        SendPacket(&packet);
    }
}

void WorldSession::LoadGlobalAccountData()
{
    LoadAccountData(
        CharacterDatabase.PQuery("SELECT type, time, data FROM account_data WHERE account='%u'", GetAccountId()),
        GLOBAL_CACHE_MASK
);
}

void WorldSession::LoadAccountData(QueryResult_AutoPtr result, uint32 mask)
{
    for (uint32 i = 0; i < NUM_ACCOUNT_DATA_TYPES; ++i)
        if (mask & (1 << i))
            m_accountData[i] = AccountData();

    if (!result)
        return;

    do
    {
        Field *fields = result->Fetch();

        uint32 type = fields[0].GetUInt32();
        if (type >= NUM_ACCOUNT_DATA_TYPES)
        {
            sLog.outError("Table `%s` have invalid account data type (%u), ignore.",
                mask == GLOBAL_CACHE_MASK ? "account_data" : "character_account_data", type);
            continue;
        }

        if ((mask & (1 << type)) == 0)
        {
            sLog.outError("Table `%s` have non appropriate for table  account data type (%u), ignore.",
                mask == GLOBAL_CACHE_MASK ? "account_data" : "character_account_data", type);
            continue;
        }

        m_accountData[type].Time = fields[1].GetUInt32();
        m_accountData[type].Data = fields[2].GetCppString();

    } while (result->NextRow());
}

void WorldSession::SetAccountData(AccountDataType type, time_t time_, std::string data)
{
    if ((1 << type) & GLOBAL_CACHE_MASK)
    {
        uint32 acc = GetAccountId();

        CharacterDatabase.BeginTransaction ();
        CharacterDatabase.PExecute("DELETE FROM account_data WHERE account='%u' AND type='%u'", acc, type);
        CharacterDatabase.escape_string(data);
        CharacterDatabase.PExecute("INSERT INTO account_data VALUES ('%u','%u','%u','%s')", acc, type, (uint32)time_, data.c_str());
        CharacterDatabase.CommitTransaction ();
    }
    else
    {
        // _player can be NULL and packet received after logout but m_GUID still store correct guid
        if (!m_GUIDLow)
            return;

        CharacterDatabase.BeginTransaction ();
        CharacterDatabase.PExecute("DELETE FROM character_account_data WHERE guid='%u' AND type='%u'", m_GUIDLow, type);
        CharacterDatabase.escape_string(data);
        CharacterDatabase.PExecute("INSERT INTO character_account_data VALUES ('%u','%u','%u','%s')", m_GUIDLow, type, (uint32)time_, data.c_str());
        CharacterDatabase.CommitTransaction ();
    }

    m_accountData[type].Time = time_;
    m_accountData[type].Data = data;
}

void WorldSession::SendAccountDataTimes(uint32 mask)
{
    WorldPacket data(SMSG_ACCOUNT_DATA_TIMES, 4+1+4+8*4); // changed in WotLK
    data << uint32(time(NULL));                             // unix time of something
    data << uint8(1);
    data << uint32(mask);                                   // type mask
    for (uint32 i = 0; i < NUM_ACCOUNT_DATA_TYPES; ++i)
        if (mask & (1 << i))
            data << uint32(GetAccountData(AccountDataType(i))->Time);// also unix time
    SendPacket(&data);
}

void WorldSession::LoadTutorialsData()
{
    for (int aX = 0 ; aX < 8 ; ++aX)
        m_Tutorials[ aX ] = 0;

    QueryResult_AutoPtr result = CharacterDatabase.PQuery("SELECT tut0,tut1,tut2,tut3,tut4,tut5,tut6,tut7 FROM character_tutorial WHERE account = '%u'", GetAccountId());

    if (result)
    {
        do
        {
            Field *fields = result->Fetch();

            for (int iI = 0; iI < 8; ++iI)
                m_Tutorials[iI] = fields[iI].GetUInt32();
        }
        while (result->NextRow());
    }
    m_TutorialsChanged = false;
}

void WorldSession::SendTutorialsData()
{
    WorldPacket data(SMSG_TUTORIAL_FLAGS, 4*8);
    for (uint32 i = 0; i < 8; ++i)
        data << m_Tutorials[i];
    SendPacket(&data);
}

void WorldSession::SaveTutorialsData()
{
    if (!m_TutorialsChanged)
        return;

    uint32 Rows=0;
    // it's better than rebuilding indexes multiple times
    QueryResult_AutoPtr result = CharacterDatabase.PQuery("SELECT count(*) AS r FROM character_tutorial WHERE account = '%u'", GetAccountId());
    if (result)
        Rows = result->Fetch()[0].GetUInt32();

    if (Rows)
    {
        CharacterDatabase.PExecute("UPDATE character_tutorial SET tut0='%u', tut1='%u', tut2='%u', tut3='%u', tut4='%u', tut5='%u', tut6='%u', tut7='%u' WHERE account = '%u'",
            m_Tutorials[0], m_Tutorials[1], m_Tutorials[2], m_Tutorials[3], m_Tutorials[4], m_Tutorials[5], m_Tutorials[6], m_Tutorials[7], GetAccountId());
    }
    else
    {
        CharacterDatabase.PExecute("INSERT INTO character_tutorial (account,tut0,tut1,tut2,tut3,tut4,tut5,tut6,tut7) VALUES ('%u', '%u', '%u', '%u', '%u', '%u', '%u', '%u', '%u')", GetAccountId(), m_Tutorials[0], m_Tutorials[1], m_Tutorials[2], m_Tutorials[3], m_Tutorials[4], m_Tutorials[5], m_Tutorials[6], m_Tutorials[7]);
    }

    m_TutorialsChanged = false;
}

void WorldSession::ReadMovementInfo(WorldPacket &data, MovementInfo *mi)
{
    data >> mi->flags;
    data >> mi->unk1;
    data >> mi->time;
    data >> mi->x;
    data >> mi->y;
    data >> mi->z;
    data >> mi->o;

    if (mi->flags & MOVEMENTFLAG_ONTRANSPORT)
    {
        if (!data.readPackGUID(mi->t_guid))
            return;

        data >> mi->t_x;
        data >> mi->t_y;
        data >> mi->t_z;
        data >> mi->t_o;
        data >> mi->t_time;
        data >> mi->t_seat;
    }

    if ((mi->flags & (MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_FLYING)) || (mi->unk1 & 0x20))
    {
        data >> mi->s_pitch;
    }

    data >> mi->fallTime;

    if (mi->flags & MOVEMENTFLAG_JUMPING)
    {
        data >> mi->j_zspeed;
        data >> mi->j_sinAngle;
        data >> mi->j_cosAngle;
        data >> mi->j_xyspeed;
    }

    if (mi->flags & MOVEMENTFLAG_SPLINE)
    {
        data >> mi->u_unk1;
    }
}

void WorldSession::WriteMovementInfo(WorldPacket *data, MovementInfo *mi)
{
    data->appendPackGUID(mi->guid);

    *data << mi->flags;
    *data << mi->unk1;
    *data << mi->time;
    *data << mi->x;
    *data << mi->y;
    *data << mi->z;
    *data << mi->o;

    if (mi->HasMovementFlag(MOVEMENTFLAG_ONTRANSPORT))
    {
       data->appendPackGUID(mi->t_guid);

        *data << mi->t_x;
        *data << mi->t_y;
        *data << mi->t_z;
        *data << mi->t_o;
        *data << mi->t_time;
        *data << mi->t_seat;
    }

    if ((mi->HasMovementFlag(MovementFlags(MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_FLYING))) || (mi->unk1 & 0x20))
    {
        *data << mi->s_pitch;
    }

    *data << mi->fallTime;

    if (mi->HasMovementFlag(MOVEMENTFLAG_JUMPING))
    {
        *data << mi->j_zspeed;
        *data << mi->j_sinAngle;
        *data << mi->j_cosAngle;
        *data << mi->j_xyspeed;
    }

    if (mi->HasMovementFlag(MOVEMENTFLAG_SPLINE))
    {
        *data << mi->u_unk1;
    }
}

void WorldSession::ReadAddonsInfo(WorldPacket &data)
{
    if (data.rpos() + 4 > data.size())
        return;
    uint32 size;
    data >> size;

    if (!size)
        return;

    if (size > 0xFFFFF)
    {
        sLog.outError("WorldSession::ReadAddonsInfo addon info too big, size %u", size);
        return;
    }

    uLongf uSize = size;

    uint32 pos = data.rpos();

    ByteBuffer addonInfo;
    addonInfo.resize(size);

    if (uncompress(const_cast<uint8*>(addonInfo.contents()), &uSize, const_cast<uint8*>(data.contents() + pos), data.size() - pos) == Z_OK)
    {
        uint32 addonsCount;
        addonInfo >> addonsCount;                         // addons count

        for (uint32 i = 0; i < addonsCount; ++i)
        {
            std::string addonName;
            uint8 enabled;
            uint32 crc, unk1;

            // check next addon data format correctness
            if (addonInfo.rpos()+1 > addonInfo.size())
                return;

            addonInfo >> addonName;

            addonInfo >> enabled >> crc >> unk1;

            sLog.outDetail("ADDON: Name: %s, Enabled: 0x%x, CRC: 0x%x, Unknown2: 0x%x", addonName.c_str(), enabled, crc, unk1);

            AddonInfo addon(addonName, enabled, crc, 2, true);

            SavedAddon const* savedAddon = sAddonMgr.GetAddonInfo(addonName);
            if (savedAddon)
            {
                bool match = true;

                if (addon.CRC != savedAddon->CRC)
                    match = false;

                if (!match)
                    sLog.outDetail("ADDON: %s was known, but didn't match known CRC (0x%x)!", addon.Name.c_str(), savedAddon->CRC);
                else
                    sLog.outDetail("ADDON: %s was known, CRC is correct (0x%x)", addon.Name.c_str(), savedAddon->CRC);
            }
            else
            {
                sAddonMgr.SaveAddon(addon);

                sLog.outDetail("ADDON: %s (0x%x) was not known, saving...", addon.Name.c_str(), addon.CRC);
            }

            // TODO: Find out when to not use CRC/pubkey, and other possible states.
            m_addonsList.push_back(addon);
        }

        uint32 currentTime;
        addonInfo >> currentTime;
        sLog.outDebug("ADDON: CurrentTime: %u", currentTime);

        if (addonInfo.rpos() != addonInfo.size())
            sLog.outDebug("packet under-read!");
    }
    else
        sLog.outError("Addon packet uncompress error!");
}

void WorldSession::SendAddonsInfo()
{
    uint8 addonPublicKey[256] =
    {
        0xC3, 0x5B, 0x50, 0x84, 0xB9, 0x3E, 0x32, 0x42, 0x8C, 0xD0, 0xC7, 0x48, 0xFA, 0x0E, 0x5D, 0x54,
        0x5A, 0xA3, 0x0E, 0x14, 0xBA, 0x9E, 0x0D, 0xB9, 0x5D, 0x8B, 0xEE, 0xB6, 0x84, 0x93, 0x45, 0x75,
        0xFF, 0x31, 0xFE, 0x2F, 0x64, 0x3F, 0x3D, 0x6D, 0x07, 0xD9, 0x44, 0x9B, 0x40, 0x85, 0x59, 0x34,
        0x4E, 0x10, 0xE1, 0xE7, 0x43, 0x69, 0xEF, 0x7C, 0x16, 0xFC, 0xB4, 0xED, 0x1B, 0x95, 0x28, 0xA8,
        0x23, 0x76, 0x51, 0x31, 0x57, 0x30, 0x2B, 0x79, 0x08, 0x50, 0x10, 0x1C, 0x4A, 0x1A, 0x2C, 0xC8,
        0x8B, 0x8F, 0x05, 0x2D, 0x22, 0x3D, 0xDB, 0x5A, 0x24, 0x7A, 0x0F, 0x13, 0x50, 0x37, 0x8F, 0x5A,
        0xCC, 0x9E, 0x04, 0x44, 0x0E, 0x87, 0x01, 0xD4, 0xA3, 0x15, 0x94, 0x16, 0x34, 0xC6, 0xC2, 0xC3,
        0xFB, 0x49, 0xFE, 0xE1, 0xF9, 0xDA, 0x8C, 0x50, 0x3C, 0xBE, 0x2C, 0xBB, 0x57, 0xED, 0x46, 0xB9,
        0xAD, 0x8B, 0xC6, 0xDF, 0x0E, 0xD6, 0x0F, 0xBE, 0x80, 0xB3, 0x8B, 0x1E, 0x77, 0xCF, 0xAD, 0x22,
        0xCF, 0xB7, 0x4B, 0xCF, 0xFB, 0xF0, 0x6B, 0x11, 0x45, 0x2D, 0x7A, 0x81, 0x18, 0xF2, 0x92, 0x7E,
        0x98, 0x56, 0x5D, 0x5E, 0x69, 0x72, 0x0A, 0x0D, 0x03, 0x0A, 0x85, 0xA2, 0x85, 0x9C, 0xCB, 0xFB,
        0x56, 0x6E, 0x8F, 0x44, 0xBB, 0x8F, 0x02, 0x22, 0x68, 0x63, 0x97, 0xBC, 0x85, 0xBA, 0xA8, 0xF7,
        0xB5, 0x40, 0x68, 0x3C, 0x77, 0x86, 0x6F, 0x4B, 0xD7, 0x88, 0xCA, 0x8A, 0xD7, 0xCE, 0x36, 0xF0,
        0x45, 0x6E, 0xD5, 0x64, 0x79, 0x0F, 0x17, 0xFC, 0x64, 0xDD, 0x10, 0x6F, 0xF3, 0xF5, 0xE0, 0xA6,
        0xC3, 0xFB, 0x1B, 0x8C, 0x29, 0xEF, 0x8E, 0xE5, 0x34, 0xCB, 0xD1, 0x2A, 0xCE, 0x79, 0xC3, 0x9A,
        0x0D, 0x36, 0xEA, 0x01, 0xE0, 0xAA, 0x91, 0x20, 0x54, 0xF0, 0x72, 0xD8, 0x1E, 0xC7, 0x89, 0xD2
    };

    WorldPacket data(SMSG_ADDON_INFO, 4);

    for (AddonsList::iterator itr = m_addonsList.begin(); itr != m_addonsList.end(); ++itr)
    {
        data << uint8(itr->State);

        uint8 crcpub = itr->UsePublicKeyOrCRC;
        data << uint8(crcpub);
        if (crcpub)
        {
            uint8 usepk = (itr->CRC != STANDARD_ADDON_CRC); // If addon is Standard addon CRC
            data << uint8(usepk);
            if (usepk)                                      // if CRC is wrong, add public key (client need it)
            {
                sLog.outDetail("ADDON: CRC (0x%x) for addon %s is wrong (does not match expected 0x%x), sending pubkey",
                    itr->CRC, itr->Name.c_str(), STANDARD_ADDON_CRC);

                data.append(addonPublicKey, sizeof(addonPublicKey));
            }

            data << uint32(/*itr->CRC*/ 0);                 // TODO: Find out the meaning of this.
        }

        uint8 unk3 = 0;                                     // 0 is sent here
        data << uint8(unk3);
        if (unk3)
        {
            // String, length 256 (null terminated)
            data << uint8(0);
        }
    }

    m_addonsList.clear();

    uint32 count = 0;
    data << uint32(count);
    /*for (uint32 i = 0; i < count; ++i)
    {
        uint32
        string (16 bytes)
        string (16 bytes)
        uint32
        uint32
    }*/

    SendPacket(&data);
}

void WorldSession::SetPlayer(Player *plr)
{
    _player = plr;

    // set m_GUID that can be used while player loggined and later until m_playerRecentlyLogout not reset
    if (_player)
        m_GUIDLow = _player->GetGUIDLow();
}
