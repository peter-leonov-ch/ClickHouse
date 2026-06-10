CREATE TABLE tab
(
  col Array(String),
  INDEX idx col TYPE text(tokenizer=array)
)
ENGINE=MergeTree() ORDER BY tuple()
AS SELECT []
