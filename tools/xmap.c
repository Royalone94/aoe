#include <sys/mman.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define DOS_MAGIC 0x5a4d
#define PE_MAGIC 0x00004550

struct mz {
	uint16_t e_magic;
	uint16_t e_cblp;
	uint16_t e_cp;
	uint16_t e_crlc;
	uint16_t e_cparhdr;
	uint16_t e_minalloc;
	uint16_t e_maxalloc;
	uint16_t e_ss;
	uint16_t e_sp;
	uint16_t e_csum;
	uint16_t e_ip;
	uint16_t e_cs;
	uint16_t e_lfarlc;
	uint16_t e_ovno;
};

struct dos {
	struct mz mz;
	uint16_t e_res[4];
	uint16_t e_oemid;
	uint16_t e_oeminfo;
	uint16_t e_res2[10];
	uint16_t e_lfanew;
};

struct pehdr {
	uint32_t f_magic;
	uint16_t f_mach;
	uint16_t f_nscns;
	uint32_t f_timdat;
	uint32_t f_symptr;
	uint32_t f_nsyms;
	uint16_t f_opthdr;
	uint16_t f_flags;
};

struct coffopthdr {
	uint16_t o_magic;
	uint16_t o_vstamp;
	uint32_t o_tsize;
	uint32_t o_dsize;
	uint32_t o_bsize;
	uint32_t o_entry;
	uint32_t o_text;
	uint32_t o_data;
	uint32_t o_image;
};

struct peopthdr {
	struct coffopthdr o_chdr;
	uint32_t o_alnsec;
	uint32_t o_alnfile;
	uint16_t o_osmajor;
	uint16_t o_osminor;
	uint16_t o_imajor;
	uint16_t o_iminor;
	uint16_t o_smajor;
	uint16_t o_sminor;
	uint32_t o_vw32;
	uint32_t o_isize;
	uint32_t o_hsize;
	uint32_t o_chksum;
	uint16_t o_sub;
	uint16_t o_dllflags;
	uint32_t o_sres;
	uint32_t o_scomm;
	uint32_t o_hres;
	uint32_t o_hcomm;
	uint32_t o_ldflags;
	uint32_t o_nrvasz;
};

struct sechdr {
	char s_name[8];
	uint32_t s_paddr;
	uint32_t s_vaddr;
	uint32_t s_size;
	uint32_t s_scnptr;
	uint32_t s_relptr;
	uint32_t s_lnnoptr;
	uint16_t s_nreloc;
	uint16_t s_nlnno;
	uint32_t s_flags;
};

#define NRVASZ_MAX 16

#define XT_UNKNOWN 0
#define XT_MZ 1
#define XT_DOS 2
#define XT_PE 3
#define XT_PEOPT 4

struct xfile {
	unsigned type;
	char *data;
	size_t size;
	struct mz *mz;
	struct dos *dos;
	struct pehdr *pe;
	struct peopthdr *peopt;
};

void xstat(struct xfile *this, char *data, size_t size)
{
	this->type = XT_UNKNOWN;
	this->mz = NULL;
	this->dos = NULL;
	this->pe = NULL;
	this->peopt = NULL;
	if (size < sizeof(struct mz)) {
		fputs("bad dos header: file too small\n", stderr);
		return;
	}
	this->type = XT_MZ;
	struct mz *mz = (struct mz*)data;
	this->mz = mz;
	if (mz->e_magic != DOS_MAGIC)
		fprintf(stderr, "bad dos magic: got %hX, %hX expected\n", mz->e_magic, DOS_MAGIC);
	size_t start = mz->e_cparhdr * 16;
	if (start >= size) {
		fprintf(stderr, "bad exe start: got %zX, max: %zX\n", start, size - 1);
		return;
	}
	off_t data_start = mz->e_cp * 512;
	if (mz->e_cblp) {
		data_start -= 512 - mz->e_cblp;
		if (data_start < 0) {
			fprintf(stderr, "bad data_start: -%zX\n", -data_start);
			return;
		}
	}
	printf("data start: %zX\n", data_start);
	if (start < sizeof(struct dos)) {
		printf("dos start: %zX\n", start);
		return;
	}
	if (start >= size) {
		fputs("bad dos start: file too small\n", stderr);
		return;
	}
	this->type = XT_DOS;
	struct dos *dos = (struct dos*)data;
	this->dos = dos;
	printf("dos start: %zX\n", start);
	size_t pe_start = dos->e_lfanew;
	if (pe_start >= size) {
		fprintf(stderr, "bad pe start: got %zX, max: %zX\n", start, size - 1);
		return;
	}
	printf("pe start: %zX\n", pe_start);
	if (size < sizeof(struct pehdr) + pe_start) {
		fputs("bad pe/coff header: file too small\n", stderr);
		return;
	}
	this->type = XT_PE;
	struct pehdr *phdr = (struct pehdr*)(data + pe_start);
	this->pe = phdr;
	if (phdr->f_magic != PE_MAGIC)
		fprintf(stderr, "bad pe magic: got %X, expected %X\n", phdr->f_magic, PE_MAGIC);
	if (!phdr->f_opthdr)
		return;
	if (size < sizeof(struct pehdr) + pe_start + sizeof(struct peopthdr)) {
		fputs("bad pe/coff header: file too small\n", stderr);
		return;
	}
	this->type = XT_PEOPT;
	struct peopthdr *pohdr = (struct peopthdr*)(data + pe_start + sizeof(struct pehdr));
	this->peopt = pohdr;
	printf("opthdr size: %hX\n", phdr->f_opthdr);
	switch (pohdr->o_chdr.o_magic) {
		case 0x10b:
			puts("type: portable executable 32 bit");
			break;
		case 0x20b:
			puts("type: portable executable 64 bit");
			break;
		default:
			printf("type: unknown: %hX\n", pohdr->o_chdr.o_magic);
			break;
	}
	if (pohdr->o_nrvasz > NRVASZ_MAX)
		fputs("rva max exceeded\n", stderr);
	printf("nrvasz: %X\n", pohdr->o_nrvasz);
}

static int process(char *name)
{
	int ret = 1, fd = -1;
	size_t mapsz;
	char *map = MAP_FAILED;
	fd = open(name, O_RDONLY);
	struct stat st;
	struct xfile x;
	if (fd == -1 || fstat(fd, &st) == -1) {
		perror(name);
		goto fail;
	}
	map = mmap(NULL, mapsz = st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) {
		perror("mmap");
		goto fail;
	}
	xstat(&x, map, mapsz);
	ret = 0;
fail:
	if (map != MAP_FAILED)
		munmap(map, mapsz);
	if (fd != -1)
		close(fd);
	return ret;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fputs("usage: xmap file\n", stderr);
		return 1;
	}
	for (int i = 1; i < argc; ++i) {
		puts(argv[i]);
		if (process(argv[i]))
			return 1;
	}
	return 0;
}
