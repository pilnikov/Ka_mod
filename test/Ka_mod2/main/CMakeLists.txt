idf_component_register(
	SRCS
	"app_main.c"
	"vs1053.c"
	"webclient.c"

	"../components/audio_player/audio_player.c"
	
	"../components/fifo/spiram_fifo.c"
	
)

include_directories(

	"./include/"
	
	"../components/audio_player/"

	"../components/fifo/"

	"../components/audio_player/include/"

	"../components/fifo/include/"

	
)