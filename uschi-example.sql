BEGIN TRANSACTION;
INSERT OR IGNORE INTO instr (sym, descr) VALUES ("UMD", "unsermarkt dosh");
INSERT OR IGNORE INTO instr (sym, descr) VALUES ("BAB", "Blood AB");
INSERT OR IGNORE INTO instr (sym, descr) VALUES ("B0", "Blood 0");
INSERT OR IGNORE INTO instr (sym, descr) VALUES ("BA", "Blood A");
INSERT OR IGNORE INTO instr (sym, descr) VALUES ("BB", "Blood B");
COMMIT;

BEGIN TRANSACTION;
INSERT OR IGNORE INTO agent (nick) VALUES ("BAB MM");
INSERT OR IGNORE INTO agent (nick) VALUES ("B0 MM");
INSERT OR IGNORE INTO agent (nick) VALUES ("BA MM");
INSERT OR IGNORE INTO agent (nick) VALUES ("BB MM");
INSERT OR IGNORE INTO agent (nick) VALUES ("tilmarkt");
COMMIT;

