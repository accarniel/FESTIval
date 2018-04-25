--this function generates spatial points (i.e., geometry objects) that intersects in at least one spatial object stored in the target_table.target_table_geo
CREATE OR REPLACE FUNCTION generate_correlated_point(base_region geometry, target_table text, target_table_geo text, num integer)
  RETURNS boolean AS 
  $BODY$
DECLARE
    	generated_point GEOMETRY;
	is_valid BOOLEAN;
	i INTEGER := 1;
	random_id INTEGER;
	max_id INTEGER;
BEGIN
	--we get a random geometry from the target table
	EXECUTE 'select max(id) from ' || target_table || ''
		INTO max_id;

	WHILE (i <= num) LOOP	
		is_valid := false;	
		WHILE (is_valid = false) LOOP
			EXECUTE 'select trunc(random() * ' || max_id || ' + 1)'
				INTO random_id;

			EXECUTE 'SELECT ST_PointOnSurface(' || target_table_geo || ') FROM ' || target_table || ' WHERE id = $1'
				INTO generated_point
				USING random_id;

			--does the generated point inside our base region?
			--note that the base region might not necessarily contain spatial objects in the target table
			IF (ST_Contains(base_region, generated_point) = '1') THEN
				is_valid := true;
			END IF;
		END LOOP;

		--insert the composed rectangle in our table
		INSERT INTO generated_point(geom, is_correlated, dataset) VALUES (generated_point, true, target_table);
		i := i + 1;
	END LOOP;
	RETURN true;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;
