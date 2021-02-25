/*
 * Write cairo FORMAT_A1 framebuffer to Leurocom M20
 * via Beaglebone Black mmap'ed GPIO with a cavalier approach
 *
 * Note: GPIO pins should be configured separately, see: start script
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <syslog.h>

#define FB_ADDR "/run/caprica/display"	// Unix domain socket string
#define HPANELS 12		// Number of LED panel columns in display
#define VPANELS 6		// Number of LED panel rows in display
#define PL 12			// LED panel height/width
#define DW (HPANELS*PL)		// Display width in pixels
#define DH (VPANELS*PL)		// Display height in pixels
#define FBW 5			// Frame buffer width in uint32_t
#define Q 3			// LED panel rows per icard
#define Z (VPANELS/Q)		// Number of icards
#define L HPANELS		// Number of LED panel columns
#define SRNB (PL/4)		// Horizontal nibbles per LED panel
#define SRVP (PL/2)		// Vertical pixels per nibble column
#define CARDOFT (FBW*Q*PL)	// Offset in 32bit words from icard to icard
#define REGOFT (SRVP*FBW)	// Offset in 32bit words between LED registers
#define DROPUSER "caprica"	// Username of unprivileged user for daemon
#define MAXFRAMEDROP 10		// force re-draw after dropping frames

/* GPIO addresses and control lines */
#define GPIOLEN 4096
#define GPIO0_ADDR 0x44e07000	// Note: Check these offsets _extremely_
#define GPIO1_ADDR 0x4804c000	// carefully before executing as root
#define GPIO_CLR (0x190/4)	// ref: SPRUH73P Table 2-3. L4_PER
#define GPIO_SET (0x194/4)	//      and Table 25-5. GPIO Registers
#define PIN_D8 2		// gpio0:2      P9.22
#define PIN_D7 3		// gpio0:3      P9.21
#define PIN_D6 12		// gpio0:12     P9.20
#define PIN_D5 13		// gpio0:13     P9.19
#define PIN_D4 4		// gpio0:18     P9.18
#define PIN_D3 5		// gpio0:5      P9.17
#define PIN_D2 31		// gpio0:31     P9.13 (not used)
#define PIN_D1 11		// gpio0:11     P9.11 (not used)
#define PIN_T 15		// gpio0:15     P9.24
#define PIN_E0 14		// gpio0:14     P9.26
#define PIN_E1 28		// gpio1:28     P9.12
#define PIN_E2 16		// gpio1:16     P9.15
#define PIN_S 19		// gpio1.19     P9.16
#define ACT 21			// gpio1.21     n/a
#define DMASK ((1<<PIN_D8)|(1<<PIN_D7)|(1<<PIN_D6)|(1<<PIN_D5)|(1<<PIN_D4)|(1<<PIN_D3)|(1<<PIN_D2)|(1<<PIN_D1))

static volatile uint32_t *gpio0_set = NULL;
static volatile uint32_t *gpio0_clr = NULL;
static volatile uint32_t *gpio1_set = NULL;
static volatile uint32_t *gpio1_clr = NULL;

/* Display addressing helper functions */

void card_first(void)
{
	/* Toggle E2 on/off */
	(*gpio1_set) = (1 << PIN_E2);
	(*gpio1_clr) = (1 << PIN_E2);
}

void card_next(void)
{
	/* Set E1 then toggle E2 on/off */
	(*gpio1_set) = (1 << PIN_E1) | (1 << ACT);
	(*gpio1_set) = (1 << PIN_E2);
	(*gpio1_clr) = (1 << PIN_E1) | (1 << PIN_E2) | (1 << ACT);
}

void strobe_t(void)
{
	/* Toggle T on/off - transfer one bit into display rows */
	(*gpio0_set) = (1 << PIN_T);
	(*gpio0_clr) = (1 << PIN_T);
}

void row_first(void)
{
	/* Set E0 to put icard in latch mode, then clear data lines */
	(*gpio0_set) = (1 << PIN_E0);
	(*gpio0_clr) = DMASK;

	/* latch data lines on icard */
	strobe_t();

	/* set D4 then release the latch mode by clearing E0 */
	(*gpio0_set) = (1 << PIN_D4);
	(*gpio0_clr) = (1 << PIN_E0);
}

void row_next(void)
{
	/* Set E0 to put icard in latch mode, then set data lines */
	(*gpio0_set) = (1 << PIN_E0);
	(*gpio0_clr) = (1 << PIN_D4) | (1 << PIN_D3);
	(*gpio0_set) =
	    (1 << PIN_D8) | (1 << PIN_D7) | (1 << PIN_D6) | (1 << PIN_D5);

	/* latch data lines on icard */
	strobe_t();

	/* set D4 then release the latch mode by clearing E0 */
	(*gpio0_set) = (1 << PIN_D4);
	(*gpio0_clr) = (1 << PIN_E0);
}

void strobe_s(void)
{
	/* Toggle S on/off - latch LED registers across whole display */
	(*gpio1_set) = (1 << PIN_S) | (1 << ACT);
	(*gpio1_clr) = (1 << PIN_S) | (1 << ACT);
}

/* Transfer bitmap pixels into LED panel shift registers */
void redraw(uint32_t * buf)
{
	uint32_t *oft = &buf[0];
	uint32_t c = 0;
	uint32_t p = 0;
	uint32_t i = 0;
	uint32_t pof = 0;
	uint32_t nbof = 0;
	uint32_t pxh = 0;
	uint32_t nbbase = 0;
	uint32_t nbshift = 0;
	uint32_t nbline = 0;
	uint32_t line = 0;
	uint32_t b8 = 0;
	uint32_t b7 = 0;
	uint32_t b6 = 0;
	uint32_t b5 = 0;
	uint32_t b4 = 0;
	uint32_t b3 = 0;
	//uint32_t b2 = 0;
	//uint32_t b1 = 0;
	uint32_t tmp = 0;

	card_first();
	while (c < Z)		// for each icard, bottom to top
	{
		oft = &buf[CARDOFT * (Z - c - 1)];
		row_first();
		p = 0;
		while (p < L)	// for each panel column, right to left
		{
			pof = L - p - 1;
			nbof = 0;
			while (nbof < SRNB)	// for each nibble column on panel
			{
				pxh = pof * PL + nbof * 4;
				nbbase = pxh / 32;
				nbshift = pxh % 32;
				line = 0;
				while (line < SRVP) {
					nbline = nbbase + line * FBW;
					b8 = oft[nbline] >> nbshift;
					b7 = oft[nbline + REGOFT] >> nbshift;
					b6 = oft[nbline +
						 2 * REGOFT] >> nbshift;
					b5 = oft[nbline +
						 3 * REGOFT] >> nbshift;
					b4 = oft[nbline +
						 4 * REGOFT] >> nbshift;
					b3 = oft[nbline +
						 5 * REGOFT] >> nbshift;
					// b2 = oft[nbline + 7 * REGOFT] >> nbshift;
					// b1 = oft[nbline + 7 * REGOFT] >> nbshift;
					for (i = 0; i < 4; i++) {
						strobe_t();
						tmp = (((b8 & 0x1) << PIN_D8)
						       | ((b7 & 0x1) << PIN_D7)
						       | ((b6 & 0x1) << PIN_D6)
						       | ((b5 & 0x1) << PIN_D5)
						       | ((b4 & 0x1) << PIN_D4)
						       | ((b3 & 0x1) <<
							  PIN_D3));
						(*gpio0_set) = tmp;
						(*gpio0_clr) = ~tmp;
						b8 >>= 1;
						b7 >>= 1;
						b6 >>= 1;
						b5 >>= 1;
						b4 >>= 1;
						b3 >>= 1;
						// b2 >>= 1;
						// b1 >>= 1;
					}
					line++;
				}
				nbof++;
			}
			p++;
			row_next();
		}
		c++;
		card_next();
	}
	strobe_s();
}

/* Print simple error and exit */
void perror_exit(char *message)
{
	perror(message);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int res = 0;

	/* map GPIO memory pages and assign registers */
	int s = open("/dev/mem", O_RDWR | O_SYNC);
	if (s == -1)
		perror_exit("Error accessing /dev/mem");
	uint32_t *gpio0 =
	    (uint32_t *) mmap(NULL, GPIOLEN, PROT_READ | PROT_WRITE,
			      MAP_SHARED, s, GPIO0_ADDR);
	if (gpio0 == MAP_FAILED)
		perror_exit("Error mapping gpio0");
	uint32_t *gpio1 =
	    (uint32_t *) mmap(NULL, GPIOLEN, PROT_READ | PROT_WRITE,
			      MAP_SHARED, s, GPIO1_ADDR);
	if (gpio1 == MAP_FAILED)
		perror_exit("Error mapping gpio1");
	if (close(s) == -1)
		perror_exit("Error closing /dev/mem");

	/* fetch uid and gid of unprivileged user */
	struct passwd *pwd = getpwnam(DROPUSER);
	if (pwd == NULL)
		perror_exit("Error fetching user and group ids");

	/* drop all privs */
	if (setgroups(0, NULL) == -1)
		perror_exit("Error dropping supplementary groups");
	if (setgid(pwd->pw_gid) != 0)
		perror_exit("Unable to drop group privileges");
	if (setuid(pwd->pw_uid) != 0)
		perror_exit("Unable to drop user privileges");

	/* set umask */
	umask(0007);

	/* save pointers to the interesting registers */
	gpio0_set = &gpio0[GPIO_SET];
	gpio0_clr = &gpio0[GPIO_CLR];
	gpio1_set = &gpio1[GPIO_SET];
	gpio1_clr = &gpio1[GPIO_CLR];

	/* request memory for framebuffer and initialise */
	size_t fblen = FBW * DH * sizeof(uint32_t);
	uint32_t *fb = calloc(FBW * DH, sizeof(uint32_t));
	if (fb == NULL)
		perror_exit("Error allocating framebuffer");

	/* Create unix datagram socket and set non-blocking */
	s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s == -1)
		perror_exit("Error creating unix socket");
	res = fcntl(s, F_GETFL);
	if (res == -1)
		perror_exit("Error reading socket flags");
	res = fcntl(s, F_SETFL, res | O_NONBLOCK);
	if (res == -1)
		perror("Error setting socket non-blocking");

	/* Clear the address and copy in socket pathname */
	struct sockaddr_un name;
	memset(&name, 0, sizeof(struct sockaddr_un));
	name.sun_family = AF_UNIX;
	strncpy(name.sun_path, FB_ADDR, sizeof(name.sun_path) - 1);

	/* unlink pathname, ignoring errors - the error is deferred to bind */
	unlink(FB_ADDR);

	/* bind address to socket - with race condition on path create */
	res =
	    bind(s, (const struct sockaddr *)&name, sizeof(struct sockaddr_un));
	if (res == -1)
		perror_exit("Error binding socket address");

	/* detach process and close all open fds */
	res = daemon(0, 1);
	if (res == -1)
		perror_exit("Error detaching process");
	int maxfd = sysconf(_SC_OPEN_MAX);
	if (maxfd == -1)
		perror_exit("Error reading _SC_OPEN_MAX");
	int fd = 0;
	while (fd < maxfd) {
		if (fd != s)
			close(fd);	// in this particular case, ignore errors
		fd++;
	}

	/* report startup to syslog */
	syslog(LOG_USER | LOG_INFO,
	       "Display: %dx%d px, %dx%d panels on %d icards, %d rows/card\n",
	       DW, DH, HPANELS, VPANELS, Z, Q);
	syslog(LOG_USER | LOG_INFO, "Listening on UNIX:%s\n", FB_ADDR);

	/* prepare display interface card, and clear display */
	(*gpio1_clr) = (1 << PIN_E1) | (1 << PIN_E2);
	redraw(&fb[0]);

	/* wait for updates on socket */
	struct pollfd pfd;
	pfd.fd = s;
	pfd.events = POLLIN;
	for (;;) {
		res = poll(&pfd, 1, -1);
		if (res == -1) {
			syslog(LOG_USER | LOG_ERR,
			       "Fatal error waiting on socket: %m");
			exit(EXIT_FAILURE);
		}
		if (pfd.revents & POLLIN) {
			res = 0;
			while (recvfrom(s, fb, fblen, 0, NULL, NULL) > 0) {
				res++;
				if (res > MAXFRAMEDROP) {
					syslog(LOG_USER | LOG_NOTICE,
					       "Dropped %d frames before update",
					       res);
					break;
				}
			}
			if (res > 0) {
				/* redraw frame buffer */
				redraw(&fb[0]);
			}
		}
	}
	return 0;
}
