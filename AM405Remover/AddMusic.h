#include <cstdio>

class AddMusic
{
public:
	AddMusic(FILE *fp, unsigned char *ROMBuf, unsigned int ROMFileSize) :		// // //
		ROMfp(fp),
		ROM(ROMBuf),
		ROMsize(ROMFileSize)
	{
	}

	int initialize();

private:
	int Remove();
	int RemoveLevelMusic(unsigned int p, int musicnum, int offset);
	int RemoveMiscMusic(unsigned int low, unsigned int middle, unsigned int high);

	unsigned int RemoveRATS(unsigned int address);

private:
	FILE *ROMfp;
	unsigned char *ROM;
	unsigned int ROMsize;

	unsigned int TableAddress;
	unsigned int OWMusicAddress;
};
