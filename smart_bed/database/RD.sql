CREATE TABLE IF NOT EXISTS beds (
  id           TEXT PRIMARY KEY,
  status       TEXT DEFAULT 'empty',
  weight       FLOAT DEFAULT 0,
  allotted_at  TIMESTAMP WITH TIME ZONE,
  vacated_at   TIMESTAMP WITH TIME ZONE,
  updated_at   TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc', now())
);


INSERT INTO beds (id, status, weight)
VALUES ('bed_01', 'empty', 0)
ON CONFLICT (id) DO NOTHING;


ALTER TABLE pesticide_logs REPLICA IDENTITY FULL;

ALTER TABLE beds REPLICA IDENTITY FULL;

Then go to:

Supabase → Database → Replication → 0 tables
  → Toggle ON: pesticide_logs
  → Toggle ON: beds


  CREATE OR REPLACE FUNCTION sync_bed_status()
RETURNS TRIGGER AS $$
BEGIN
  IF NEW.status = 'occupied' THEN
    UPDATE beds SET
      status      = 'occupied',
      weight      = NEW.weight,
      allotted_at = timezone('utc', now()),
      vacated_at  = NULL,
      updated_at  = timezone('utc', now())
    WHERE id = 'bed_01';

  ELSIF NEW.status = 'empty' THEN
    UPDATE beds SET
      status     = 'empty',
      weight     = NEW.weight,
      vacated_at = timezone('utc', now()),
      updated_at = timezone('utc', now())
    WHERE id = 'bed_01';
  END IF;

  RETURN NEW;
END;
$$ LANGUAGE plpgsql;



CREATE OR REPLACE TRIGGER on_new_log
AFTER INSERT ON pesticide_logs
FOR EACH ROW
EXECUTE FUNCTION sync_bed_status();


-- Manually test the trigger
INSERT INTO pesticide_logs (weight, status)
VALUES (120.5, 'occupied');

-- Check if beds updated automatically
SELECT * FROM beds WHERE id = 'bed_01';


INSERT INTO pesticide_logs (weight, status)
VALUES (10.0, 'empty');

SELECT * FROM beds WHERE id = 'bed_01';
