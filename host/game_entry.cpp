// ****************************************************************************
//
//                       Host runtime - game entry shim
//
// ****************************************************************************
// Games are C++; their main() is renamed by -Dmain=game_main, which gives a
// C++-mangled int game_main(). The host runtime (C) calls the C function
// host_call_game_main(). Added to game targets only (picopad_add_game), not to the host
// object library: C unit tests define host_call_game_main() themselves.

int game_main();

extern "C" int host_call_game_main(void)
{
	return game_main();
}
