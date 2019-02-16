# Log File Format

## Format Modes
|Mode|Requirement|
|-|-|
|16-bit|(x ∈ {channel_name_size, username_data_size, message_data_size, number_of_lines}, x <= 0xFFFF) ∧ file size <= 0xFFFFFFFFFFFFFFFF|
|32-bit|(x ∈ {channel_name_size, username_data_size, message_data_size, number_of_lines}, x <= 0xFFFFFFFF) ∧ file size <= 0xFFFFFFFFFFFFFFFF|
|64-bit|(x ∈ {channel_name_size, username_data_size, message_data_size, number_of_lines}, x <= 0xFFFFFFFFFFFFFFFF) ∧ file size <= 0xFFFFFFFFFFFFFFFF|

## Header

|16-bit offset|32-bit offset|64-bit offset|16-bit size|32-bit size|64-bit size|Field|Description| 
|-|-|-|-|-|-|-|-|
|0x00|0x00|0x00|1|1|1|format_mode|Format of file, 1 = 16-bit, 2 = 32-bit, 3 = 64-bit.|
|0x01|0x01|0x01|1|1|1|endianness|Endianess of file, 1 = bigendian, 2 = little endian|
|0x02|0x02|0x02|6|6|6|Unused|Unused|
|0x08|0x08|0x08|8|8|8|channel_name_offset||
|0x10|0x10|0x10|8|8|8|username_data_offset||
|0x18|0x18|0x18|8|8|8|message_data_offset||
|0x20|0x20|0x20|8|8|8|timepoint_list_offset||
|0x28|0x28|0x28|8|8|8|username_list_offset||
|0x30|0x30|0x30|8|8|8|message_list_offset||
|0x32|0x34|0x38|2|4|8|channel_name_size||
|0x34|0x38|0x40|2|4|8|username_data_size||
|0x36|0x3C|0x48|2|4|8|message_data_size||
|0x38|0x40|0x50|2|4|8|number_of_lines||

* All values are unsigned.
* Offsets in the header are relative to the file start.
* Sizes are given in bytes.

## Data

|Offset field|16-bit element size|32-bit element size|64-bit element size|Description|
|-|-|-|-|-|
|channel_name_offset|1|1|1|Start of channel name string, size is given in channel_name_size.|
|username_data_offset|1|1|1|Start of username data (See [Username data layout](#username-data-layout)), Size is given in username_data_size.|
|message_data_offset|1|1|1|Start of message data.|
|timepoint_list_offset|8|8|8|Element index corresponds to what message it belongs to. Element is a timepoint. Time points are always 64 bit posix time. size = element size * number_of_lines.|
|username_list_offset|2|4|8|Element index corresponds to what message it belongs to. Element is a offset relative to username_data_offset. size = element size * number_of_lines.|
|message_list_offset|4|8|16|Element index corresponds to what message it belongs to. (See [Message list layout](#message-list-layout)), size = element size * number_of_lines.|

### Username data layout

|16-bit offset|32-bit offset|64-bit offset|16-bit size|32-bit size|64-bit size|Field|Description|
|-|-|-|-|-|-|-|-|
|0x0|0x0|0x0|2|4|8|size|Size of string.|
|0x2|0x4|0x8|size|size|size|string_ptr|Start of string.|

### Message list layout

|16-bit offset|32-bit offset|64-bit offset|16-bit size|32-bit size|64-bit size|Field|Description|
|-|-|-|-|-|-|-|-|
|0x0|0x0|0x0|2|4|8|message_offset|Offset relative to message_data_offset.|
|0x2|0x4|0x8|2|4|8|message_size|Size of message.|