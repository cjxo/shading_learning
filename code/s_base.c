function String_Const_U8
str8_make(char *c, u64 length) {
	String_Const_U8 result;
	result.str = (u8 *)c;
	result.char_count = length;
	result.char_capacity = length;
	return(result);
}
