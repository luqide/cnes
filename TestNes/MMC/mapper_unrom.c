
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "cnes.h"
#include "mapper.h"

#define	CHR_RAM_SIZE	0x2000

struct unrom_context {
	uint8_t mirror;
	uint32_t banksel;	/* PRG bank */

	uint8_t chr_ram[CHR_RAM_SIZE];
};

static uint8_t
unrom_cpuread(struct cnes_context *av, uint16_t addr)
{
	struct unrom_context *mc = av->mmc.priv;

#ifdef UNROM_DEBUG
	printf("[%s] addr=$%04X\n", __func__, addr);
#endif

	if (addr >= 0x8000 && addr <= 0xBFFF) {
		/* 16KB switchable PRG ROM bank */
		return av->rom_data[av->prg_start + (addr - 0x8000) + (mc->banksel * 0x4000)];
	}

	if (addr >= 0xC000 && addr <= 0xFFFF) {
		/* 16KB PRG ROM bank, fixed to the last bank */
		return av->rom_data[av->prg_start + (addr - 0xC000) + (av->prg_len - 0x4000)];
	}

	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
	return 0;
}

static void
unrom_cpuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	struct unrom_context *mc = av->mmc.priv;

#ifdef UNROM_DEBUG
	printf("[%s] addr=$%04X val=$%02X\n", __func__, addr, val);
#endif

	if (addr >= 0x8000 && addr <= 0xFFFF) {
		/* Bank select */
		mc->banksel = val;
		return;
	}

	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
}

static uint8_t
unrom_ppuread(struct cnes_context *av, uint16_t addr)
{
	struct unrom_context *mc = av->mmc.priv;
	//uint8_t val;

#ifdef UNROM_DEBUG
	printf("[%s] addr=$%04X\n", __func__, addr);
#endif

	if (addr >= 0x0000 && addr <= 0x1FFF) {
		if (av->chr_len)
			return av->rom_data[av->chr_start + addr];
		else
			return mc->chr_ram[addr];
	}

	if (addr >= 0x2000 && addr <= 0x3EFF) {
		/* Nametable */
		if (addr >= 0x3000)
			addr -= 0x1000;

		off_t off;
		switch (mc->mirror) {
		case ROM_MIRROR_H:
			off = (addr & 0x3FF) + (addr < 0x2800 ? 0 : 0x400);
			break;
		case ROM_MIRROR_V:
			off = (addr & 0x7FF);
			break;
		default:
			assert("Unsupported mirror mode" == NULL);
		}
		return av->vram[off];
	}

	printf("[%s] PPU address $%04X not mapped\n", __func__, addr);
	return 0;
}

static void
unrom_ppuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	struct unrom_context *mc = av->mmc.priv;

#ifdef UNROM_DEBUG
	printf("[%s] addr=$%04X val=$%02X\n", __func__, addr, val);
#endif

	if (av->chr_len == 0 && addr >= 0x0000 && addr <= 0x1FFF) {
		mc->chr_ram[addr] = val;
		return;
	}

	if (addr >= 0x2000 && addr <= 0x3EFF) {
		/* Nametable */
		if (addr >= 0x3000)
			addr -= 0x1000;

		off_t off;
		switch (mc->mirror) {
		case ROM_MIRROR_H:
			off = (addr & 0x3FF) + (addr < 0x2800 ? 0 : 0x400);
			break;
		case ROM_MIRROR_V:
			off = (addr & 0x7FF);
			break;
		default:
			assert("Unsupported mirror mode" == NULL);
		}
		av->vram[off] = val;
		return;
	}

	printf("[%s] PPU address $%04X not mapped\n", __func__, addr);
}

static int
unrom_init(mapper_context *m, const uint8_t *data, size_t datalen)
{
	struct unrom_context *mc;

	mc = calloc(1, sizeof(*mc));
	if (mc == NULL)
		return ENOMEM;

	mc->mirror = data[6] & ROM_MIRROR_MASK;

	m->cpuread = unrom_cpuread;
	m->cpuwrite = unrom_cpuwrite;
	m->ppuread = unrom_ppuread;
	m->ppuwrite = unrom_ppuwrite;
	m->priv = mc;

	return 0;
}

MAPPER_DECL(unrom, "UNROM", unrom_init);
