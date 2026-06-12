WITH
    
    44100 AS sample_frequency
    , number AS tick
    , tick / sample_frequency AS time

    
    , (time, wave, delay_, decay, count) -> arraySum(n1 -> wave(time - delay_ * n1), range(count)) AS delay

    , delay(time, (time -> 0.5), 0.2, 0.5, 5) AS kick

SELECT

    kick

FROM system.numbers
LIMIT 5
