-- Migration: create firmware bucket and firmware_updates table
begin;

-- Create public bucket for OTA binaries
select storage.create_bucket('firmware', public := true);

-- Table to track available firmware
create table firmware_updates (
  id bigserial primary key,
  version text not null,
  binary_url text not null,
  created_at timestamp with time zone default now()
);

-- Allow ESP devices to read the latest firmware
alter table firmware_updates enable row level security;
create policy "Allow public read" on firmware_updates
  for select using (true);

-- Insert initial firmware row
insert into firmware_updates (version, binary_url)
values ('1.1.0',
  'https://qymcapixzxuvzcgdjksq.supabase.co/storage/v1/object/public/firmware/chatbox-v1.1.0.bin');

commit;
