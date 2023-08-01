#pragma once

enum ClientFrameStage_t {
	FRAME_UNDEFINED = -1,
	FRAME_START,
	FRAME_NET_UPDATE_START,
	FRAME_NET_UPDATE_POSTDATAUPDATE_START,
	FRAME_NET_UPDATE_POSTDATAUPDATE_END,
	FRAME_NET_UPDATE_END,
	FRAME_RENDER_START,
	FRAME_RENDER_END
};

class BaseClient {
public:
	virtual void Function0();

	virtual void Function1();

	virtual void Function2();

	virtual void Function3();

	virtual void Function4();

	virtual void Function5();

	virtual void Function6();

	virtual void Function7();

	virtual void Function8();

	virtual void Function9();

	virtual void Function10();

	virtual void Function11();

	virtual void Function12();

	virtual void Function13();

	virtual void Function14();

	virtual void Function15();

	virtual void Function16();

	virtual void Function17();

	virtual void Function18();

	virtual void Function19();

	virtual void Function20();

	virtual void Function21();

	virtual void Function22();

	virtual void Function23();

	virtual void Function24();

	virtual void Function25();

	virtual void Function26();

	virtual void Function27();

	virtual void Function28();

	virtual void Function29();

	virtual void Function30();

	virtual void Function31();

	virtual void Function32();

	virtual void Function33();

	virtual void Function34();

	virtual void FrameStageNotify(ClientFrameStage_t curStage) = 0;
};