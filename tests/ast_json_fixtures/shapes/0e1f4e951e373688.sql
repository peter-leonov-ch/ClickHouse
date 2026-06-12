CREATE TABLE tab
(
    s Array(String),
    INDEX idx s TYPE text(tokenizer = sparseGrams),
    PROJECTION p (SELECT s ORDER BY s)
)
ENGINE = MergeTree() ORDER BY tuple()
