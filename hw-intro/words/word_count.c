/*

Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/

#include "word_count.h"
// #define DEBUG
/* Basic utilities */

char* new_string(char* str) {
  char* new_str = (char*)malloc(strlen(str) + 1);
  if (new_str == NULL) {
    return NULL;
  }
  return strcpy(new_str, str);
}

int init_words(WordCount** wclist) {
  /* Initialize word count.
     Returns 0 if no errors are encountered
     in the body of this function; 1 otherwise.
  */
  if (wclist == NULL) {
    printf("ERROR:init_words() pass in a NULL pointer instead of &(*WordCount)\n");
    return 1;
  }
  *wclist = NULL;
  return 0;
}

ssize_t len_words(WordCount* wchead) {
  /* Return -1 if any errors are
     encountered in the body of
     this function.
  */
  //Empty head node
  if (wchead == NULL)
    return 0;
  size_t len = 0;
  //Start loop
  while (wchead != NULL) {
    if (wchead->count == 0) {
      printf("ERROR:len_words() the freq of word %s is 0.\n", wchead->word);
      return -1;
    }
    len++;
    wchead = wchead->next;
  }
  return len;
}

WordCount* find_word(WordCount* wchead, char* word) {
/* Return count for word, if it exists */
#ifdef DEBUG
  printf("FUNCTION:find_word\n");
#endif
  WordCount* wc = wchead;
  while (wc) {
#ifdef DEBUG
    printf("curNode:%s\n", wc->word);
#endif
    if (strcmp(wc->word, word) == 0) {
      break;
    }
    wc = wc->next;
  }
  return wc;
}

int add_word(WordCount** wclist, char* word) {
#ifdef DEBUG
  printf("FUNCTION:add_word\n");
#endif
  /* If word is present in word_counts list, increment the count.
     Otherwise insert with count 1.
     Returns 0 if no errors are encountered in the body of this function; 1 otherwise.
  */

  //Add a new head
  if (*wclist == NULL) {
    *wclist = malloc(sizeof(WordCount));
    if (*wclist == NULL) {
      printf("ERROR:add_word() fails to malloc a new header\n");
      return 1;
    }
    (*wclist)->word = word;
    (*wclist)->count = 1;
    (*wclist)->next = NULL;
#ifdef DEBUG
    printf("Insert the header:%s with freq%d\n", (*wclist)->word, (*wclist)->count);
#endif
    return 0;
  }
  //Search
  WordCount* cur = find_word(*wclist, word);
  //Not present
  if (cur == NULL) {
    WordCount* temp = *wclist;
    while (temp->next != NULL)
      temp = temp->next;
    temp->next = malloc(sizeof(WordCount));
    if (temp->next == NULL) {
      printf("ERROR:add_word() fails to malloc a new node at tail\n");
      return 1;
    }
    temp = temp->next;
    temp->word = word;
    temp->count = 1;
    temp->next = NULL;
#ifdef DEBUG
    printf("Insert the tail:%s with freq%d\n", temp->word, temp->count);
#endif
  } else {
    cur->count++;
#ifdef DEBUG
    printf("Increment the word:%s with freq:%d\n", cur->word, cur->count);
#endif
  }
  return 0;
}

void fprint_words(WordCount* wchead, FILE* ofile) {
  /* print word counts to a file */
  WordCount* wc;
  for (wc = wchead; wc; wc = wc->next) {
    fprintf(ofile, "%i\t%s\n", wc->count, wc->word);
  }
}
