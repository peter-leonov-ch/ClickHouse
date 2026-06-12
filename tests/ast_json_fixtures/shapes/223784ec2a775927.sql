CREATE TABLE tab
(
    str String,
    INDEX idx str TYPE text() 
)
ENGINE = MergeTree
ORDER BY tuple()
