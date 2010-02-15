DELETE FROM `spell_proc_event` WHERE `entry` IN (28716, 31571, 31572, 55672, 31871, 31872, 33076, 33953, 48110, 48111, 48112, 48113, 33076, 44745, 54787, 47535, 47536, 47537, 51483, 51485, 51486, 56372, 56374, 61356, 41635);
INSERT INTO `spell_proc_event` (`entry`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `procFlags`, `procEx`, `ppmRate`, `CustomChance`, `Cooldown`) VALUES
( 28716, 0x00,   7, 0x00000010, 0x00000000, 0x00000000, 0x00040000, 0x00000000,   0,   0,   0), -- Rejuvenation
( 31571, 0x00,   3, 0x00000000, 0x00000022, 0x00000000, 0x00004000, 0x00000000,   0,   0,   0), -- Arcane Potency (Rank 1)
( 31572, 0x00,   3, 0x00000000, 0x00000022, 0x00000000, 0x00004000, 0x00000000,   0,   0,   0), -- Arcane Potency (Rank 2)
( 31871, 0x00,  10, 0x00000010, 0x00000000, 0x00000000, 0x00004000, 0x00000000,   0,   0,   0), -- Divine Purpose (Rank 1)
( 31872, 0x00,  10, 0x00000010, 0x00000000, 0x00000000, 0x00004000, 0x00000000,   0,   0,   0), -- Divine Purpose (Rank 2)
( 33076, 0x00,   0, 0x00000000, 0x00000000, 0x00000000, 0x000A22A8, 0x00000000,   0,   0,   0), -- Prayer of Mending (Rank 1)
( 33953, 0x00,   0, 0x00000000, 0x00000000, 0x00000000, 0x00044000, 0x00000000,   0,   0,  45), -- Essence of Life 
( 48110, 0x00,   0, 0x00000000, 0x00000000, 0x00000000, 0x000A22A8, 0x00000000,   0,   0,   0), -- Prayer of Mending (Rank 2)
( 48111, 0x00,   0, 0x00000000, 0x00000000, 0x00000000, 0x000A22A8, 0x00000000,   0,   0,   0), -- Prayer of Mending (Rank 3)
( 48112, 0x00,   0, 0x00000000, 0x00000000, 0x00000000, 0x000A22A8, 0x00000000,   0,   0,   0), -- Prayer of Mending (Rank 2)
( 48113, 0x00,   0, 0x00000000, 0x00000000, 0x00000000, 0x000A22A8, 0x00000000,   0,   0,   0), -- Prayer of Mending (Rank 3)
( 41635, 0x00,   0, 0x00000000, 0x00000000, 0x00000000, 0x000A22A8, 0x00000000,   0,   0,   0), -- Prayer of Mending (Rank 1)
( 44745, 0x00,   3, 0x00000000, 0x00000001, 0x00000000, 0x00004000, 0x00002000,   0,   0,   0), -- Shattered Barrier (Rank 1)
( 54787, 0x00,   3, 0x00000000, 0x00000001, 0x00000000, 0x00004000, 0x00002000,   0,   0,   0), -- Shattered Barrier (Rank 2)
( 47535, 0x00,   6, 0x00000001, 0x00000000, 0x00000000, 0x00004000, 0x00002000,   0,   0,  12), -- Rapture (Rank 1)
( 47536, 0x00,   6, 0x00000001, 0x00000000, 0x00000000, 0x00004000, 0x00002000,   0,   0,  12), -- Rapture (Rank 2)
( 47537, 0x00,   6, 0x00000001, 0x00000000, 0x00000000, 0x00004000, 0x00002000,   0,   0,  12), -- Rapture (Rank 3)
(51483, 0x00000001, 11, 0x20000000, 0x00000000, 0x00000000, 0x00004000, 0x00000000, 0.000000, 0.000000,  0),
(51485, 0x00000001, 11, 0x20000000, 0x00000000, 0x00000000, 0x00004000, 0x00000000, 0.000000, 0.000000,  0),
(51486, 0x00000001, 11, 0x20000000, 0x00000000, 0x00000000, 0x00004000, 0x00000000, 0.000000, 0.000000,  0),
( 56372, 0x00,   3, 0x00000000, 0x00000080, 0x00000000, 0x00004000, 0x00000000,   0,   0,   0), -- Glyph of Ice Block
( 56374, 0x00,   3, 0x00000000, 0x00004000, 0x00000000, 0x00004000, 0x00000000,   0,   0,   0), -- Glyph of Icy Veins 
( 61356, 0x00,   0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000002,   0,   0,  90); -- Invigorating Earthsiege Diamond Passive
