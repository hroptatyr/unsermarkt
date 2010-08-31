CREATE TABLE 'order' (
  order_id INTEGER PRIMARY KEY AUTOINCREMENT
);

CREATE TABLE 'agent' (
  agent_id INTEGER PRIMARY KEY AUTOINCREMENT,
  nick VARCHAR(64) UNIQUE
);

CREATE TABLE 'instr' (
  instr_id INTEGER PRIMARY KEY AUTOINCREMENT,
  sym VARCHAR(16) UNIQUE,
  descr VARCHAR(64)
);

-- agent portfolio, fact
CREATE TABLE 'agtinv' (
  agent_id INTEGER,
  instr_id INTEGER,
  -- denominator is 10000
  lpos INTEGER,
  spos INTEGER,
  PRIMARY KEY (agent_id, instr_id),
  FOREIGN KEY (agent_id) REFERENCES 'agent' (agent_id),
  FOREIGN KEY (instr_id) REFERENCES 'instr' (instr_id)
);

CREATE TABLE 'match' (
  match_id INTEGER PRIMARY KEY AUTOINCREMENT,
  b_agent_id INTEGER,
  s_agent_id INTEGER,
  b_instr_id INTEGER,
  s_instr_id INTEGER,
  price NUMERIC,
  quantity NUMERIC,
  FOREIGN KEY (b_agent_id) REFERENCES 'agent' (agent_id)  
  FOREIGN KEY (s_agent_id) REFERENCES 'agent' (agent_id)  
  FOREIGN KEY (b_instr_id) REFERENCES 'instr' (instr_id)  
  FOREIGN KEY (s_instr_id) REFERENCES 'instr' (instr_id)  
);

