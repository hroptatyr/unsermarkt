#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>

#define PGSZ		(4096)
#define ROUND_PGSZ(_x)	(((_x) / PGSZ + 1) * PGSZ)
#define MAP_MEM		(MAP_PRIVATE | MAP_ANONYMOUS)
#define PROT_MEM	(PROT_READ | PROT_WRITE)

typedef struct mmls_s *mmls_t;
typedef struct mmls_cell_s *mmls_cell_t;

struct mmls_s {
	mmls_cell_t head;
	uint32_t len;
	uint32_t csz;
	char data[];
};

/* that's how we imagine cells look like */
struct mmls_cell_s {
	mmls_cell_t next;
	char data[];
};


static void
__chain_mmls(mmls_t m, size_t len, size_t cell_size)
{
	size_t cszp = cell_size / sizeof(void*);
	mmls_cell_t p = m->head;
	void *ep = (char*)m + len;

	for (mmls_cell_t o = p + cszp; o < ep; p->next = o, p = o, o += cszp);
	p->next = NULL;
	return;
}

static mmls_t
make_mmls(size_t cell_size, size_t count)
{
	mmls_t res;
	uint32_t len = ROUND_PGSZ(count * cell_size + sizeof(*res));

	res = mmap(NULL, len, PROT_MEM, MAP_MEM, 0, 0);
	res->len = len;
	res->csz = (uint32_t)cell_size;
	res->head = (void*)((char*)res + sizeof(*res));
	/* chain the stuff together initially */
	__chain_mmls(res, len, cell_size);
	return res;
}

static void
free_mmls(mmls_t m)
{
	munmap(m, m->len);
	return;
}

static void*
mmls_pop_cell(mmls_t m)
{
	mmls_cell_t res = m->head;
	m->head = res->next;
	/* clear out the old next pointer */
	res->next = NULL;
	return (void*)res;
}

static void
mmls_push_cell(mmls_t m, void *cell)
{
	mmls_cell_t c = (void*)cell;
	c->next = m->head;
	m->head = c;
	return;
}

/* mmls.c ends here */
