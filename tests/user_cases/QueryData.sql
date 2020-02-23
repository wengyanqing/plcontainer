DROP VIEW IF EXISTS usercase.table_view;
CREATE OR REPLACE VIEW usercase.table_view AS
WITH ca1 AS (
  SELECT date, 
         sum(MARKET_CAP) AS TOTAL_MARKET_CAP
  FROM usercase.table_input_data 
  GROUP BY date
),
cj1 AS (
  SELECT ID, 
         i.DATE, 
         RETURNS, 
         MARKET_CAP, 
         TOTAL_MARKET_CAP,
         MARKET_CAP/TOTAL_MARKET_CAP AS BENCHMARK_WEIGHT
  FROM usercase.table_input_data i, CA1 c
  WHERE i.DATE = c.DATE
),
ca2 AS (
  SELECT date, 
         sum(BENCHMARK_WEIGHT*RETURNS) AS BENCHMARK_RETURN
  FROM cj1 
  GROUP BY DATE
)
SELECT ID, 
       j.DATE, 
       RETURNS, 
       MARKET_CAP, 
       TOTAL_MARKET_CAP,
       BENCHMARK_WEIGHT, 
       BENCHMARK_RETURN
FROM cj1 j, ca2 a
WHERE  j.DATE=a.DATE;


DROP TYPE IF EXISTS usercase.usertype CASCADE;
CREATE TYPE usercase.usertype AS (
  asof_date date, 
  beta double precision
); 
                 

DROP FUNCTION IF EXISTS usercase.userfunc(date[], double precision[], double precision[], double precision[], double precision[], double precision[], integer);
CREATE OR REPLACE FUNCTION usercase.userfunc(
  date date[]
 ,returns double precision[]
 ,market_cap double precision[]
 ,total_market_cap double precision[]
 ,benchmark_weight double precision[]
 ,benchmark_return double precision[]
 ,rollingwindowsize integer
) RETURNS SETOF usercase.usertype AS
$$
# container: plc_r_shared
  userfunc <- function(df, rolling_window_size=rollingwindowsize) {

    library(zoo)
  
    compute.capm=function(ts,window_size)
    {
      #initialize return
      rr<-zoo()
      rr <- tryCatch(
      {
        rollapply(ts, width = window_size,
        FUN = function(z) coef(lm.fit(y=as.vector(z[,1]),x=cbind(1,as.matrix(z[,2])))),
        by.column = FALSE, align = "right");
      },
      error=function(cond) { return(zoo()) }
      )
      return (rr)
    }
    
    #process input
    my_ts <- zoo(cbind(df[,2],df[,6]),zoo:::as.Date(df[,1]))
    outDF <- compute.capm(my_ts,rolling_window_size)
    final <- NULL
    if (length(outDF) >0) {
      outDF2 <- data.frame(asof_date=index(outDF),coredata(outDF))
      final <- data.frame(
       as.character(as.Date(outDF2$asof_date))
      ,as.double(outDF2$x2)
      )
     colnames(final) = c("ASOF_DATE","BETA")
    }
    return(final)
  }

  df <- data.frame(
    date=date
   ,returns=returns
   ,market_cap=market_cap
   ,total_market_cap=total_market_cap
   ,benchmark_weight=benchmark_weight
   ,benchmark_return=benchmark_return
  )
  
  return(userfunc(df))
  
$$ LANGUAGE plcontainer;

drop table if exists usercase.table_output; 
			   
create table usercase.table_output
--WITH (appendonly=true, compresstype=quicklz) --ZO laptop space constrained :)
WITH (appendonly=true) --ZO laptop space constrained :)
AS
SELECT id
      ,(usercase.userfunc(date,returns,market_cap,total_market_cap,benchmark_weight,benchmark_return,100)).*
FROM (
  SELECT id
        ,array_agg(date::date ORDER BY date) AS date
        ,array_agg(returns ORDER BY date) AS returns
        ,array_agg(market_cap ORDER BY date) AS market_cap
        ,array_agg(total_market_cap ORDER BY date) AS total_market_cap 
        ,array_agg(benchmark_weight ORDER BY date) AS benchmark_weight
        ,array_agg(benchmark_return ORDER BY date) AS benchmark_return
  FROM usercase.table_view 
  --WHERE id IN (1,2,3)
  --AND date_part('year',date) = 1994
  GROUP BY id
) foo
ORDER BY 1,2
Distributed by (id);
					
