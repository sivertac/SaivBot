# Log File Format

|32-bit offset|64-bit offset|32-bit size|64-bit size|Field|Description| 
|-|-|-|-|-|-|
|0x0|0x0|1|1|format_type|Format of file, 1 = 32-bit, 2 = 64-bit.|
|0x1|0x1|1|1|endianness|Endianess of file, 1 = bigendian, 2 = little endian|
|0x2|0x2|6|6|Unused|Unused|
|0x8|0x8|4|8|channel_name_offset||
|0xC|0x10|4|8|channel_name_size||
|0x10|0x18|4|8|username_data_offset||
|0x14|0x20|4|8|username_data_size||
|0x18|0x28|4|8|message_data_offset||
|0x1C|0x30|4|8|message_data_size||
|0x20|0x38|4|8|timepoint_data_offset||
|0x24|0x40|4|8|username_list_offset||
|0x28|0x48|4|8|message_list_offset||
|0x2C|0x50|4|8|number_of_lines||

