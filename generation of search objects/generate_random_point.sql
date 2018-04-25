CREATE OR REPLACE FUNCTION generate_random_point(base_region geometry, num integer, ds text default 'ALL')
  RETURNS boolean AS 
  $BODY$
DECLARE
  generated_point GEOMETRY;
	is_valid BOOLEAN;
	xmin FLOAT; xmax FLOAT;
	ymin FLOAT; ymax FLOAT;
	x FLOAT; y FLOAT;
	i INTEGER := 1;
BEGIN

	xmin := (SELECT ST_XMin(base_region)); 	xmax := (SELECT ST_XMax(base_region));
	ymin := (SELECT ST_YMin(base_region)); 	ymax := (SELECT ST_YMax(base_region));

	WHILE (i <= num) LOOP	
		is_valid := false;	
		WHILE (is_valid = false) LOOP
			--we create random x and y coordinates inside of our extent
			x := (SELECT xmin + ROUND(CAST (random()*(xmax - xmin) AS NUMERIC),6));
			y := (SELECT ymin + ROUND(CAST (random()*(ymax - ymin) AS NUMERIC),6));

			generated_point := (SELECT ST_SetSRID(ST_MakePoint(x, y), st_srid(base_region)));

			--does the generated points inside our base region?
			IF (ST_Contains(base_region, generated_point) = '1') THEN
				is_valid := true;
			END IF;
		END LOOP;
		--insert the composed rectangle in our table
		INSERT INTO generated_point(geom, is_correlated, dataset) VALUES (generated_point, false, ds);
		i := i + 1;
	END LOOP;
	RETURN true;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;
