--this function generates rectangles with a relative size of the base_region object; in addition, these rectangles overlap the base_region
CREATE OR REPLACE FUNCTION generate_random_rect(base_region geometry, perc double precision, num integer, ds text default 'ALL')
  RETURNS boolean AS 
  $BODY$
DECLARE
    	generated_rect GEOMETRY;
	final_rect GEOMETRY;
	is_valid BOOLEAN;
	xmin FLOAT; xmax FLOAT;
	ymin FLOAT; ymax FLOAT;
	x FLOAT; y FLOAT;
	distance_x FLOAT; distance_y FLOAT;
	centroid GEOMETRY;
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

			generated_rect := (select ST_Envelope(ST_Scale(ST_Envelope(base_region), sqrt(perc / 100), sqrt(perc / 100), sqrt(perc / 100))));
			centroid := ST_Centroid(generated_rect);
			distance_x := x - ST_X(centroid);
			distance_y := y - ST_Y(centroid);

			--we move the generated rectangle to a random location
			final_rect := ST_Translate(generated_rect, distance_x, distance_y);
			--does the final_rect OVERLAP our region? -- we do not use the ST_Overlap here because of some problems with the resolution of geometries
			IF (ST_Intersects(final_rect, base_region) = '1' AND (ST_area(ST_intersection(final_rect, ST_Buffer(base_region, 0.0000001)))/ST_area(final_rect)) > .2) THEN
				is_valid := true;
			END IF;
		END LOOP;
		--insert the composed rectangle in our table
		INSERT INTO generated_rectangle(geom, percentage, type, is_correlated, dataset) VALUES (final_rect, perc, null, false, ds);
		i := i + 1;
	END LOOP;
	RETURN true;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;
