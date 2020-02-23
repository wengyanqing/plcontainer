CREATE SCHEMA usercase;

DROP FUNCTION IF EXISTS usercase.rnorm(double precision, double precision);
CREATE OR REPLACE FUNCTION usercase.rnorm(
  mean double precision,
  std double precision
) RETURNS double precision AS
$$
# container: plc_r_shared
  return(rnorm(n = 1, mean = mean, sd = std))
$$ LANGUAGE plcontainer;


DROP FUNCTION IF EXISTS usercase.runif(double precision, double precision);
CREATE OR REPLACE FUNCTION usercase.runif(
  min double precision,
  max double precision
) RETURNS double precision AS
$$
# container: plc_r_shared
  return(runif(n = 1, min = min, max = max))
$$ LANGUAGE plcontainer;


--adjust stock # here
DROP TABLE IF EXISTS usercase.createonestockbasetable;
CREATE TABLE usercase.createonestockbasetable AS
SELECT id
--FROM generate_series(1,10000,1) AS id
FROM generate_series(1,100,1) AS id
DISTRIBUTED BY (id);


DROP TABLE IF EXISTS usercase.createonedatetable;
CREATE TABLE usercase.createonedatetable AS
SELECT ('1984-01-01'::date + interval '1 days' * cnt)::date AS date
--FROM generate_series(1,20 * 365,1) AS cnt
FROM generate_series(1,2 * 365,1) AS cnt
DISTRIBUTED RANDOMLY;


DROP TABLE IF EXISTS usercase.table_input_data CASCADE;
CREATE TABLE usercase.table_input_data 
--WITH (appendonly=true, compresstype=quicklz) --ZO laptop space constrained :)
WITH (appendonly=true) --ZO laptop space constrained :)
AS
SELECT id
      ,date
      ,usercase.rnorm(0.0, 0.1) AS returns
      ,usercase.runif(0.0, 0.5) AS market_cap
FROM usercase.createonestockbasetable
CROSS JOIN usercase.createonedatetable
DISTRIBUTED BY (id);

Analyze usercase.table_input_data;

--on ZO's MBP:
--100 stocks took 13 seconds,  730000 rows 
--10000 stocks took 27 minutes, 73000000 rows 
