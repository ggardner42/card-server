/**
 * Shuffle a deck using Fisher-Yates algorithm, using secure random numbers.
 * (Assumes presence of /dev/urandom.)
 * The random bits are assumed to be a limited resource,
 * so care is taken to use them sparingly.
 *
 * Let's say we want a random number in the range [0-4] inclusive.
 * Then we only need 3 bits to get a number in that range.
 * But we can't just take "x % 5", where x is a 3 bit number,
 * because x has a range of 0-7, which is biased.
 * That is, consider all the possible values for the 3 bits (0-7),
 * and taking their remainders mod 5, so we get:
 * 0, 1, 2, 3, 4, 0, 1
 * So taking "x % 5" of all the range of 0-7 will
 * bias the result in favor of 0 and 1.
 *
 * To eliminate the bias, we have to use the group
 * "0, 1, 2, 3, 4", or the group "2, 3, 4, 0, 1".
 * The below code uses the first group, by failing
 * those numbers that fall within the latter "0, 1"
 * (the code: "defect = rmax % max; while (rmax - defect <= r) {}")
 *
 * This code is based on a stack overflow post on how to get an unbiased result:
 * https://stackoverflow.com/questions/2509679/how-to-generate-a-random-integer-number-from-within-a-range
 * but instead of throwing away the bits when r is outside the unbiased range,
 * another bit is added (and the rand max expanded).
 * This has a better chance, on average, of preserving bits.
 * Instead of throwing away, say, 6 bits that failed the bias for [0-33],
 * we add a bit to get 7 bits for the same range [0-33].
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFSZ 256
#define MAX_RAND_BITS (28)

// Get the next secure random bit, doing block reads of bits
static uint32_t secure_random_bit(void)
{
  static uint32_t rand_buf[BUFSZ];
  static uint32_t max_winx = 0;
  static uint32_t winx = 0;
  static uint32_t bcnt = 0;

  if (winx == max_winx)
  {
    ssize_t rv;
    int fd = open("/dev/urandom", O_RDONLY);

    if (fd < 0)
    {
      perror("open");
      exit(1);
    }

    rv = read(fd, rand_buf, sizeof(rand_buf));
    close(fd);

    // check we got at least one word
    if (rv < (ssize_t)sizeof(rand_buf[0]))
    {
      perror("read");
      exit(1);
    }

    max_winx = rv / sizeof(rand_buf[0]);
    winx = 0;
    bcnt = 0;
  }

  // take the lowest bit
  uint32_t bit = rand_buf[winx] & 1;

  // shift and maybe incr winx
  rand_buf[winx] >>= 1;
  bcnt++;
  if (bcnt == sizeof(rand_buf[0]) * 8)
  {
    winx++;
    bcnt = 0;
  }

  return bit;
}

// Returns in the closed interval [0, max-1], using the fewest number of random bits
static uint32_t secure_random(uint32_t max)
{
  if (max == 0)
  {
    fprintf(stderr, "secure_random() called with max == 0\n");
    exit(1);
  }

  uint32_t r = 0, rmax = 1;

  // get enough bits to be >= max
  while (rmax < max)
  {
    // put the random bit at the "front" of r
    if (secure_random_bit() == 1)
      r |= rmax;
    rmax <<= 1;
  }

  uint32_t defect = rmax % max;

  // keep adding bits until valid
  while (rmax - defect <= r)
  {
    // put the random bit at the "front" of r
    if (secure_random_bit() == 1)
      r |= rmax;

    // go for a max size of bits
    if (rmax < (1<<MAX_RAND_BITS))
    {
      rmax <<= 1;
      defect = rmax % max;
    }
    else
      // throw away lsb when at 'max' number of bits
      r >>= 1;
  }

  // Truncated division is intentional
  return r / (rmax / max);
}

#define DECK_SIZE 52

// Card representation (e.g., 0 = Ace of Spades, 51 = King of Clubs)
typedef struct
{
  char suit; // 'S', 'H', 'D', 'C'
  char rank; // 'A', '2', ..., 'K'
} Card;

// Initialize a deck of 52 cards
void init_deck(Card *deck)
{
  const char suits[] = {'S', 'H', 'D', 'C'};
  const char ranks[] = {'A', '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K'};

  for (uint32_t i = 0; i < DECK_SIZE; i++)
  {
    deck[i].suit = suits[i / 13];
    deck[i].rank = ranks[i % 13];
  }
}

// Fisher-Yates shuffle using secure random numbers
void shuffle_deck(Card *deck)
{
  for (uint32_t i = DECK_SIZE - 1; i > 0; i--)
  {
    uint32_t j = secure_random(i + 1); // Random index in [0, i]

    // Swap deck[i] and deck[j]
    Card temp = deck[i];
    deck[i] = deck[j];
    deck[j] = temp;
  }
}

// Print deck for debugging
void print_deck(Card *deck)
{
  for (uint32_t i = 0; i < DECK_SIZE; i++)
  {
    printf("%c%c ", deck[i].rank, deck[i].suit);
    if ((i + 1) % 13 == 0)
      printf("\n");
  }
}

// Test shuffle
int main(int argc __attribute__((unused)), char **argv)
{
  uint32_t uplim = strtol(argv[1], NULL, 0);
  Card deck[DECK_SIZE];

  init_deck(deck);
  printf("Original deck:\n");
  print_deck(deck);

  // shuffle a lot, to see how fast we can go.
  for (uint32_t i = 0; i < uplim; i++)
    shuffle_deck(deck);

  printf("\nShuffled deck:\n");
  print_deck(deck);

  exit(0);
}
