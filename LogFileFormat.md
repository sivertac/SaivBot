# Log File Format

## Format Modes
|Mode|Requirement|
|-|-|
|16-bit|x ∈ {channel_name_size, username_data_size, message_data_size, number_of_lines}, x <= 0xFFFF|
|32-bit|x ∈ {channel_name_size, username_data_size, message_data_size, number_of_lines}, x <= 0xFFFFFFFF|
|64-bit|x ∈ {channel_name_size, username_data_size, message_data_size, number_of_lines}, x <= 0xFFFFFFFFFFFFFFFF|

## Header

|16-bit offset|32-bit offset|64-bit offset|16-bit size|32-bit size|64-bit size|Field|Description| 
|-|-|-|-|-|-|-|-|
|0x00|0x00|0x00|1|1|1|format_mode|Format of file, 1 = 32-bit, 2 = 64-bit.|
|0x01|0x01|0x01|1|1|1|endianness|Endianess of file, 1 = bigendian, 2 = little endian|
|0x02|0x02|0x02|6|6|6|Unused|Unused|
|0x08|0x08|0x08|2|4|8|channel_name_offset||
|0x0A|0x0C|0x10|2|4|8|channel_name_size||
|0x0C|0x10|0x18|2|4|8|username_data_offset||
|0x0E|0x14|0x20|2|4|8|username_data_size||
|0x10|0x18|0x28|2|4|8|message_data_offset||
|0x12|0x1C|0x30|2|4|8|message_data_size||
|0x14|0x20|0x38|2|4|8|timepoint_data_offset||
|0x16|0x24|0x40|2|4|8|username_list_offset||
|0x18|0x28|0x48|2|4|8|message_list_offset||
|0x1A|0x2C|0x50|2|4|8|number_of_lines||

(All values are unsigned.)

## Data

|Offset field|16-bit element size|32-bit element size|64-bit element size|Description|
|-|-|-|-|-|
|channel_name_offset|1|1|1||
|username_data_offset|1|1|1||
|message_data_offset|1|1|1||
|timepoint_data_offset|8|8|8||
|username_list_offset|2|4|8||
|message_list_offset|4|8|16||


