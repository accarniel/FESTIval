--this function generated rectangles that certainly intersect at least one spatial object in the target_table.target_table_geo
CREATE OR REPLACE FUNCTION generate_correlated_qws2irqs(base_region geometry, perc double precision, target_table text, target_table_geo text, num integer)
  RETURNS boolean AS 
  $BODY$
DECLARE
  generated_rect GEOMETRY;
	random_obj GEOMETRY;
	final_rect GEOMETRY;
	is_valid BOOLEAN;
	x FLOAT; y FLOAT;
	distance_x FLOAT; distance_y FLOAT;
	centroid GEOMETRY;
	i INTEGER := 1;
	max_id INTEGER;
	random_id INTEGER;
BEGIN
	EXECUTE 'select max(id) from ' || target_table || ''
		INTO max_id;

	WHILE (i <= num) LOOP	
		is_valid := false;	
		WHILE (is_valid = false) LOOP
			--we get a random geometry from the target table
			EXECUTE 'select trunc(random() * ' || max_id || ' + 1)'
				INTO random_id;

			EXECUTE 'SELECT ' || target_table_geo || ' FROM ' || target_table || ' WHERE id = $1'
				INTO random_obj
				USING random_id;

			generated_rect := (select ST_Envelope(ST_Scale(ST_Envelope(base_region), sqrt(perc / 100), sqrt(perc / 100), sqrt(perc / 100))));
			centroid := ST_Centroid(generated_rect);
			distance_x := ST_X(ST_Centroid(random_obj)) - ST_X(centroid);
			distance_y := ST_Y(ST_Centroid(random_obj)) - ST_Y(centroid);

			--we move the generated rectangle to the location
			final_rect := ST_Translate(generated_rect, distance_x, distance_y);
			--does the final_rect INTERSECT our random object and it intersects the base_region?
			IF (ST_Intersects(final_rect, random_obj) = '1' AND ST_Intersects(final_rect, base_region) = '1') THEN
				is_valid := true;
			END IF;
		END LOOP;

		--insert the composed rectangle in our table
		INSERT INTO generated_rectangle(geom, percentage, type, is_correlated, dataset) VALUES (final_rect, perc, 'intersection', true, target_table);
		i := i + 1;
	END LOOP;
	RETURN true;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

--this function generated rectangles that certainly contain at least one spatial object in the target_table.target_table_geo
CREATE OR REPLACE FUNCTION generate_correlated_qws2crqs(base_region geometry, perc double precision, target_table text, target_table_geo text, num integer)
  RETURNS boolean AS 
  $BODY$
DECLARE
    	generated_rect GEOMETRY;
	random_obj GEOMETRY;
	final_rect GEOMETRY;
	is_valid BOOLEAN;
	x FLOAT; y FLOAT;
	distance_x FLOAT; distance_y FLOAT;
	centroid GEOMETRY;
	i INTEGER := 1;
	max_id INTEGER;
	random_id INTEGER;
	exist INTEGER;
BEGIN
	EXECUTE 'select max(id) from ' || target_table || ''
		INTO max_id;

	WHILE (i <= num) LOOP	
		is_valid := false;	
		WHILE (is_valid = false) LOOP
			--we get a random geometry from the target table
			EXECUTE 'select trunc(random() * ' || max_id || ' + 1)'
				INTO random_id;

			EXECUTE 'SELECT ' || target_table_geo || ' FROM ' || target_table || ' WHERE id = $1'
				INTO random_obj
				USING random_id;

			generated_rect := (select ST_Envelope(ST_Scale(ST_Envelope(base_region), sqrt(perc / 100), sqrt(perc / 100), sqrt(perc / 100))));
			centroid := ST_Centroid(generated_rect);
			distance_x := ST_X(ST_Centroid(random_obj)) - ST_X(centroid);
			distance_y := ST_Y(ST_Centroid(random_obj)) - ST_Y(centroid);

			--we move the generated rectangle to the location
			final_rect := ST_Translate(generated_rect, distance_x, distance_y);
			--does the final_rect contain properly an object from our target_table? it intersects the base_region?
			--the containsproperly function of postgis refers to the contains of the research papers
			EXECUTE 'SELECT count(*) FROM ' || target_table || ' WHERE ST_ContainsProperly($1, ' || target_table_geo || ')'
				INTO exist
				USING final_rect;
			
			IF (exist >= 1 AND ST_Intersects(final_rect, base_region) = '1') THEN
				is_valid := true;
			END IF;
		END LOOP;

		--insert the composed rectangle in our table
		INSERT INTO generated_rectangle(geom, percentage, type, is_correlated, dataset) VALUES (final_rect, perc, 'containment (contains)', true, target_table);
		i := i + 1;
	END LOOP;
	RETURN true;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;
