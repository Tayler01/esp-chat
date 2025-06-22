-- Migration: create firmware bucket and firmware_updates table
begin;

-- Some Supabase stacks do not expose the storage.create_bucket() helper.
-- Insert directly into storage.buckets instead to avoid issues.
insert into storage.buckets (id, name, public)
values ('firmware', 'firmware', true)
on conflict (id) do nothing;

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
