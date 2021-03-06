#include <kernel.h>

#define NF						5
#define MIN_THINK				2000
#define MAX_THINK				6000
#define MIN_EAT					2000
#define MAX_EAT					6000

#define COLOR_NORMAL			LIGHTGRAY
#define COLOR_THINKING			WHITE
#define COLOR_HUNGRY			YELLOW
#define COLOR_EATING			LIGHTRED
#define COLOR_FREE				WHITE
#define COLOR_USED				LIGHTRED

typedef struct
{
	int x;
	int y;
}
point;

// TLS

typedef struct
{
	Task_t *phil[NF];			// filósofos
	bool in_use[NF];			// tenedores usados
	bool hungry[NF];			// filósofos hambrientos
}
data;

#define phil		TLS(data)->phil
#define in_use		TLS(data)->in_use
#define hungry		TLS(data)->hungry

static int
prev(int n)
{
	return n == 0 ? NF-1 : n-1;
}

static int
next(int n)
{
	return n == NF-1 ? 0 : n+1;
}

#define left_fork(i)			(i)
#define right_fork(i)			next(i)
#define left_phil(i)			prev(i)
#define right_phil(i)			next(i)

// posiciones en pantalla de los filosofos y los tenedores
static point pos[2*NF] =
{
	{ 55, 11 },
	{ 51,  6 },
	{ 41,  2 },
	{ 28,  2 },
	{ 18,  6 },
	{ 15, 11 },
	{ 18, 17 },
	{ 28, 21 },
	{ 41, 21 },
	{ 51, 17 }
};

#define phil_position(i)		(pos[2*(i)+1])
#define fork_position(i)		(pos[2*(i)])

static void
mprint(point where, char *msg, unsigned color)
{
	Atomic();
	mt_cons_gotoxy(where.x, where.y);
	cprintk(color, BLACK, "%-10.10s", msg);
	Unatomic();
}

static void 
random_delay(int d1, int d2)
{
	unsigned t;

	Atomic();
	t = d1 + rand() % (d2-d1);
	Unatomic();

	Delay(t);
}

static void
think(int i)
{
	mprint(phil_position(i), "THINKING", COLOR_THINKING);
	random_delay(MIN_THINK, MAX_THINK);
}

static void
become_hungry(int i)
{
	mprint(phil_position(i), "HUNGRY", COLOR_HUNGRY);
}

static void
eat(int i)
{
	mprint(phil_position(i), "EATING", COLOR_EATING);
	random_delay(MIN_EAT, MAX_EAT);
}

static bool
forks_avail(int i)
{
	return !in_use[left_fork(i)] && !in_use[right_fork(i)];
}

static void
take_forks(int i)
{
	in_use[left_fork(i)] = in_use[right_fork(i)] = true;
	mprint(fork_position(left_fork(i)), "used", COLOR_USED);
	mprint(fork_position(right_fork(i)), "used", COLOR_USED);
}

static void
leave_forks(int i)
{
	in_use[left_fork(i)] = in_use[right_fork(i)] = false;
	mprint(fork_position(left_fork(i)), "free", COLOR_FREE);
	mprint(fork_position(right_fork(i)), "free", COLOR_FREE);
}

static int
philosopher(void *arg)
{
	int i;
	unsigned len = sizeof i;
	int left, right;

	Receive(NULL, &i, &len);
	left = left_phil(i);
	right = right_phil(i);

	while ( true )
	{
		think(i);
		become_hungry(i);

		Atomic();
		if ( forks_avail(i) )
			take_forks(i);
		else
		{
			hungry[i] = true;
			Pause();
			hungry[i] = false;
		}
		Unatomic();

		eat(i);

		Atomic();
		leave_forks(i);
		if ( hungry[left] && forks_avail(left) )
		{
			take_forks(left);
			Ready(phil[left]);
		}
		if ( hungry[right] && forks_avail(right) )
		{
			take_forks(right);
			Ready(phil[right]);
		}
		Unatomic();
	}
	return 0;
}

int 
atomic_phil_main(int argc, char *argv[])
{
	int i;
	bool cursor;

	// Alocar TLS
	TLS = Malloc(sizeof(data));

	// Inicializar display
	mt_cons_clear();
	cursor = mt_cons_cursor(false);
	cprintk(LIGHTGREEN, BLACK, "Oprima cualquier tecla para salir");
	for ( i = 0 ; i < NF ; i++ )
		mprint(fork_position(i), "free", COLOR_FREE);

	// Crear y arrancar tareas
	for ( i = 0 ; i < NF ; i++ )
	{
		Task_t *t = phil[i] = CreateTask(philosopher, 0, NULL, "filosofo", DEFAULT_PRIO);
		Ready(t);
		Send(t, &i, sizeof i);
	}

	// Esperar una tecla para salir
	getch();
	
	// Liquidar tareas
	for ( i = 0 ; i < NF ; i++ )
		DeleteTask(phil[i], 0);

	// Reponer pantalla
	mt_cons_clear();
	mt_cons_cursor(cursor);

	// Liberar TLS
	Free(TLS);

	return 0;
}
