// Dictionary permutations
//
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
	int count;
	void* elems[];
} list_t;

typedef struct {
	char* str;
	uint8_t len;
	uint8_t uppercased : 1;
	uint8_t lowercased : 1;
	uint8_t capitalized : 1;
} word_t;

typedef struct {
	list_t* words;
	uint8_t used : 1;
} word_group_t;

typedef struct {
	char* str;
	uint8_t len : 7;
	uint8_t used : 1;
} separator_t;

typedef struct {
	list_t* wordgroups;
	list_t* separators;
	int MAX_WORD_SIZE;
	int MAX_WORD_GROUP_SIZE;
	int MAX_SEPARATOR_SIZE;
	int MIN_PASSWORD_SIZE;
	int MAX_PASSWORD_SIZE;
	int MAX_DEPTH;

	int uppercase_version:1;
	int lowercase_version:1;
	int toggle_case_version:1;
	int capitalize_version:1;

	const char* wordgroups_file;
	const char* separators_file;

	char password[65];
} cfg_t;

void fatal(const char* message) {
	fprintf(stderr, "FATAL: %s\n", message);
	abort();
}

void list_add(list_t** plist, void* item)
{
	list_t* list = *plist;
	int cnt = list == NULL ? 1 : list->count + 1;
	size_t sz = offsetof(list_t, elems) + sizeof(void*) * cnt;
	list = (list_t*)realloc(list, sz);
	if (!list) 
		fatal("Unable to allocate memory for item");
	list->elems[cnt-1] = item;
	list->count = cnt;
	*plist = list;
}

void list_free(list_t** plist, void (*freeitem)(void*)) 
{
	if (!plist || !*plist)
		return;
	list_t* list = *plist;
	int cnt = list->count;
	for (int i = 0; i < cnt; ++i) {
		freeitem(list->elems[i]);
	}
	free(list);
	*plist = NULL;
}

void load_wordgroups(list_t** wordgroups, cfg_t* cfg)
{
	FILE* fin = fopen(cfg->wordgroups_file, "rb");
	if (!fin) {
		fprintf(stderr, "ERROR: Unable to open wordgroups file '%s'\n", cfg->wordgroups_file);
		return;
	}

	char* buf = (char*)malloc(cfg->MAX_WORD_GROUP_SIZE + 2);
	if (!buf)
		fatal("Unable to allocate memory for word group line processing");

	// Create lists for each word size
	while (!feof(fin)) {
	
		// Read a word
		fgets(buf, cfg->MAX_WORD_GROUP_SIZE, fin);
		buf[cfg->MAX_WORD_GROUP_SIZE + 1] = '\0';

		// Create a word group
		word_group_t* group = (word_group_t*)malloc(sizeof(word_group_t));
		if (!group)
			fatal("Unable to allocate memory for word group");

		group->words = NULL;
		group->used = 0;

		// Split word group into words
		bool end = false;
		char* pf = &buf[0];
		char* pe = pf;
		do {
			// Scan to the end of the word
			while (*pe > ' ') ++pe;
			// Last word ?
			end = *pe == 0;
			// End word
			*pe++ = 0;
			// If any word was split, store it!
			if (*pf != 0) {
				size_t word_len = strlen(pf);
				// And place it into our list
				if (word_len > cfg->MAX_WORD_SIZE) {
					fprintf(stderr, "ERROR: '%s' is longer (%d) than maximum allowed (%d). Dropped it\n", buf,(int) word_len,(int)cfg->MAX_WORD_SIZE);
				}
				else {
					word_t* word = (word_t*)malloc(sizeof(word_t));
					if (!word)
						fatal("Unable to allocate memory for word");

					// Check if word is capitalized, all uppercase or all lowercase
					bool capitalized = true;
					bool allupper = true;
					bool alllower = true;
					char* p = pf;
					while (1){
						char c = *p;
						if (!c) break;
						if (isalpha(c)) {
							if (!isupper(c)) 
								allupper = false;
							if (!islower(c)) 
								alllower = false;
							if ((p == pf && !isupper(c)) || (p!=pf && !islower(c)))
								capitalized=false;
						}
						p++;
					}

					// Store word and its attributes
					word->len = (uint8_t)word_len;
					word->str = strdup(pf);
					word->capitalized = capitalized;
					word->lowercased = alllower;
					word->uppercased = allupper;
					list_add(&group->words, word);
				}
			}
			// Go to the next char
			pf = pe;
		} while (!end);

		// Finally, push the word group
		list_add(wordgroups, group);

	}
	fclose(fin);
	free(buf);
}

static void free_word(void* p)
{
	word_t* word = (word_t*)p;
	free(word->str);
	free(word);
}

void free_words(list_t** words)
{
	list_free(words, free_word);
}

static void free_group(void* p)
{
	word_group_t* group = (word_group_t*)p;
	free_words(&group->words);
	free(group);
}

void free_wordgroups(list_t** wordgroups)
{
	list_free(wordgroups, free_group);
}

static void free_separator(void* p)
{
	separator_t* separator = (separator_t*)p;
	free(separator->str);
	free(separator);
}

void free_separators(list_t** separators)
{
	list_free(separators, free_separator);
}

void load_default_separators(list_t** separators, cfg_t* cfg)
{
	// By default, use an empty separator
	separator_t* separator = (separator_t*)malloc(sizeof(separator_t));
	if (!separator)
		fatal("Unable to allocate memory for word");
	separator->len = 0;
	separator->used = 0;
	separator->str = strdup("");
	list_add(separators, separator);
}

void load_separators(list_t** separators,cfg_t* cfg)
{
	FILE* fin = fopen(cfg->separators_file, "rb");
	if (!fin) {
		fprintf(stderr, "ERROR: Unable to open separator file '%s'\n", cfg->separators_file);

		// And use default separators
		load_default_separators(separators, cfg);
		return;
	}

	char* buf = (char*)malloc(cfg->MAX_SEPARATOR_SIZE + 2);
	if (!buf)
		fatal("Unable to allocate memory for word group line processing");

	while (!feof(fin)) {

		// Read a word
		fgets(buf, cfg->MAX_SEPARATOR_SIZE, fin);
		buf[cfg->MAX_SEPARATOR_SIZE + 1] = '\0';

		// Get word length
		size_t separator_len = strlen(buf);

		// Remove line terminators, but NOT spaces or tabs
		char c;
		while (separator_len > 0 && ((c=buf[separator_len - 1]) == '\r' || c == '\n')) 
			--separator_len;
		buf[separator_len] = '\0';

		// And place it into our list
		if (separator_len > cfg->MAX_SEPARATOR_SIZE) {
			fprintf(stderr, "'%s' is longer (%d) than maximum allowed (%d). Dropped it\n", buf, (int)separator_len, (int)cfg->MAX_SEPARATOR_SIZE);
		}
		else {
			separator_t* separator = (separator_t*)malloc(sizeof(separator_t));
			if (!separator)
				fatal("Unable to allocate memory for word");
			separator->len = (char)separator_len;
			separator->used = 0;
			separator->str = strdup(buf);
			list_add(separators, separator);
		}
	}
	fclose(fin);
	free(buf);

}

bool str_to_upper(char* dst, const char* src)
{
	char c;
	bool changed = false;
	while ((c = *src++) != 0)
	{
		char n = c;
		if (isalpha(c)) {
			n = toupper(c);
		}
		if (n != c)
			changed = true;
		*dst++ = n;
	}
	*dst = 0;
	return changed;
}

bool str_to_lower(char* dst, const char* src)
{
	char c;
	bool changed = false;
	while ((c = *src++) != 0)
	{
		char n = c;
		if (isalpha(c)) {
			n = tolower(c);
		}
		if (n != c)
			changed = true;
		*dst++ = n;
	}
	*dst = 0;
	return changed;
}

bool str_toggle_case(char* dst, const char* src)
{
	char c;
	bool changed = false;
	while ((c = *src++) != 0)
	{
		char n = c;
		if (isalpha(c)) {
			if (isupper(c)) {
				n = tolower(c);
			}
			else {
				n = toupper(c);
			}
		}
		if (n != c)
			changed = true;
		*dst++ = n;

	}
	*dst = 0;
	return changed;
}

extern void recurse_wordgroups(cfg_t* cfg, const int password_pos, const int depth);

void try_all_word_spellings(cfg_t* cfg, char* password, int pos, int len, const word_t* word, int word_length, const int depth)
{
	// append the word to the current prefix
	memcpy(&password[pos], word->str, word_length + 1);

	// Print generated password
	if (len >= cfg->MIN_PASSWORD_SIZE && len <= cfg->MAX_PASSWORD_SIZE)
		fprintf(stdout, "%s\n", password);

	// Recurse generation
	recurse_wordgroups(cfg, pos + word_length, depth + 1);

	// If word is not already capitalized, and asked to capitalize it.. and it is
	// possible to capitalize...
	if (!word->capitalized && cfg->capitalize_version) {

		// Capitalize 1st letter
		char org_char = password[pos];
		char capitalized = toupper(password[pos]);
		if (capitalized != org_char) {

			// And redo recursion
			password[pos] = capitalized;

			// Print generated password
			if (len >= cfg->MIN_PASSWORD_SIZE && len <= cfg->MAX_PASSWORD_SIZE)
				printf("%s\n", password);

			// Recurse generation
			recurse_wordgroups(cfg, pos + word_length, depth + 1);
		}
	}

	// If word is not already uppercase, and asked to uppercase it.. and it is
	// possible to uppercase...
	if (!word->uppercased && cfg->uppercase_version) {

		// Uppercase
		if (str_to_upper(&password[pos], word->str)) {

			// Print generated password
			if (len >= cfg->MIN_PASSWORD_SIZE && len <= cfg->MAX_PASSWORD_SIZE)
				printf("%s\n", password);

			// Recurse generation
			recurse_wordgroups(cfg, pos + word_length, depth + 1);
		}
	}

	// If word is not already lowercase, and asked to lowercase it.. and it is
	// possible to lowercase...
	if (!word->lowercased && cfg->lowercase_version) {

		// Lowercase
		if (str_to_lower(&password[pos], word->str)) {

			// Print generated password
			if (len >= cfg->MIN_PASSWORD_SIZE && len <= cfg->MAX_PASSWORD_SIZE)
				printf("%s\n", password);

			// Recurse generation
			recurse_wordgroups(cfg, pos + word_length, depth + 1);
		}
	}

	// If asked to toggle case
	if (cfg->toggle_case_version) {

		// Toggle case
		if (str_toggle_case(&password[pos], word->str)) {

			// Print generated password
			if (len >= cfg->MIN_PASSWORD_SIZE && len <= cfg->MAX_PASSWORD_SIZE)
				printf("%s\n", password);

			// Recurse generation
			recurse_wordgroups(cfg, pos + word_length, depth + 1);
		}
	}
}

void recurse_wordgroups(cfg_t* cfg,const int password_pos, const int depth)
{
	char* password = cfg->password;
	const list_t* wordgroups = cfg->wordgroups;
	const list_t* separators = cfg->separators;

	// Go wordgroup by wordgroup
	for (int wordgroups_idx = 0; wordgroups_idx < wordgroups->count; ++wordgroups_idx) {

		// Get the item
		word_group_t* group = (word_group_t*)wordgroups->elems[wordgroups_idx];

		// Only consider word if not already used in the chain
		if (!group->used) {

			// Mark word as used, so it is not reused
			group->used = 1;

			// Get the list of words
			const list_t* words = group->words;

			// Go word by word
			for (int word_idx = 0; word_idx < words->count; ++word_idx) {

				// Get the word
				const word_t* word = (const word_t*)words->elems[word_idx];

				// Get the word length
				int word_length = word->len;

				// Calculate the password length that will be generated at this level
				int password_length = password_pos + word_length;

				// Only if the password length does not exceed the maximum allowed and the depth
				// does not exceed what was specified
				if (password_length <= cfg->MAX_PASSWORD_SIZE && depth <= cfg->MAX_DEPTH) {

					// Iterate through all separators... Except in the first word
					if (depth > 1) {
						for (int sepidx = 0; sepidx < cfg->separators->count; ++sepidx) {
							const separator_t* sep = (const separator_t*)cfg->separators->elems[sepidx];

							// Add the separator
							memcpy(&password[password_pos], sep->str, sep->len);

							// Compute the word position
							int pos = password_pos + sep->len;
							int len = password_length + sep->len;

							// Try all word spellings
							try_all_word_spellings(cfg, password, pos, len, word, word_length, depth);
						}
					}
					else {
						// Compute the word position
						int pos = password_pos;
						int len = password_length;

						// Try all word spellings
						try_all_word_spellings(cfg, password, pos, len, word, word_length, depth);
					}
				}
			}

			// Mark word as unused
			group->used = 0;
		}
	}
}

void help()
{
	fprintf(stderr, "group_permutator [options] group_of_words_file [separators_file]\n");
	fprintf(stderr, "(c) 2024-2025 Eduardo José Tagle\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "options\n");
	fprintf(stderr, "--max-word-sz=XX \tMaximum word size, in characters, default: 64\n");
	fprintf(stderr, "--max-group-sz=XX\tMaximum word group size, in characters, default: 4096\n");
	fprintf(stderr, "--max-sep-sz=XX  \tMaximum separator size, in characters, default: 64\n");
	fprintf(stderr, "--min-pw-sz=XX   \tMinimum password length to generate, in characters, default: 8\n");
	fprintf(stderr, "--max-pw-sz=XX   \tMaximum password length to generate, in characters, default: 16\n");
	fprintf(stderr, "--max-depth=XX   \tMaximum count of groups to be used for each password, default: 64\n");
	fprintf(stderr, "--uc             \tAlso try the same words in uppercase, default: do not\n");
	fprintf(stderr, "--lc             \tAlso try the same words in lowercase, default: do not\n");
	fprintf(stderr, "--cc             \tAlso try the same words capitalized,  default: do not\n");
	fprintf(stderr, "--tc             \tAlso try the same words with toggled case,  default: do not\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "group_of_words_file is a file with groups of words, one group per line, words separated by spaces\n");
	fprintf(stderr, "separators_file     is an optative file with word separators to use, one separator for each line\n");
	fprintf(stderr, "\n");
	fprintf(stderr, " This utility will compute all permutations of just ONE word of each group, creating all\n");
	fprintf(stderr, "possible passwords from min-pw-sz to max-pw-size, optatively using all the possible separators\n");
	fprintf(stderr, "between permutated words.\n");
	fprintf(stderr, " It can also add case-modified versions of each word to each group.\n");
}

int main(int argc, char** argv)
{
	cfg_t cfg;
	cfg.MAX_WORD_SIZE = 64;
	cfg.MAX_WORD_GROUP_SIZE = 4096;
	cfg.MIN_PASSWORD_SIZE = 8;
	cfg.MAX_PASSWORD_SIZE = 16;
	cfg.MAX_SEPARATOR_SIZE = 64;
	cfg.MAX_DEPTH = 64;
	cfg.wordgroups = NULL;
	cfg.separators = NULL;
	cfg.uppercase_version = false;
	cfg.lowercase_version = false;
	cfg.toggle_case_version = false;
	cfg.capitalize_version = true;
	cfg.wordgroups_file = NULL; // "e:\\wordlists\\specialdict.txt";
	cfg.separators_file = NULL; // "e:\\wordlists\\separators.txt";

	// Help screen
	if (argc < 2) {
		help();
		return 1;
	}

	// Process command line
	int pos = 1;
	while (pos < argc) {
		char* arg = argv[pos];
		if (arg[0] == '-' && arg[1] == '-') {
			// Option
			if (strncmp(arg + 2, "max-word-sz=", 12) == 0)
				cfg.MAX_WORD_SIZE = atoi(arg + 2 + 12);
			else
				if (strncmp(arg + 2, "max-group-sz=", 13) == 0)
					cfg.MAX_WORD_GROUP_SIZE = atoi(arg + 2 + 13);
				else
					if (strncmp(arg + 2, "max-sep-sz=", 11) == 0)
						cfg.MAX_SEPARATOR_SIZE = atoi(arg + 2 + 11);
					else
						if (strncmp(arg + 2, "min-pw-sz=", 10) == 0)
							cfg.MIN_PASSWORD_SIZE = atoi(arg + 2 + 10);
						else
							if (strncmp(arg + 2, "max-pw-sz=", 10) == 0)
								cfg.MAX_PASSWORD_SIZE = atoi(arg + 2 + 10);
							else
								if (strncmp(arg + 2, "max-depth=", 10) == 0)
									cfg.MAX_DEPTH = atoi(arg + 2 + 10);
								else
									if (strcmp(arg + 2, "uc") == 0)
										cfg.uppercase_version = true;
									else
										if (strcmp(arg + 2, "lc") == 0)
											cfg.lowercase_version = true;
										else
											if (strcmp(arg + 2, "cc") == 0)
												cfg.capitalize_version = true;
											else
												if (strcmp(arg + 2, "tc") == 0)
													cfg.toggle_case_version = true;
												else {
													fprintf(stderr,"ERROR: Unsupported option '%s'\n", arg);
													return 1;
												}

		}
		else
		{
			if (!cfg.wordgroups_file)
				cfg.wordgroups_file = arg;
			else
				if (!cfg.separators_file)
					cfg.separators_file = arg;
				else {
					fprintf(stderr, "WARN: Ingnoring argument '%s'\n", arg);
					return 1;
				}
		}
		++pos;
	}

	// Load wordlist
	load_wordgroups(&cfg.wordgroups, &cfg);

	// Load possible separators
	if (cfg.separators_file)
		load_separators(&cfg.separators, &cfg);
	else
		load_default_separators(&cfg.separators, &cfg);

	// Now, we must generate all permutations of words that are between the specified password lenght
	recurse_wordgroups(&cfg, 0, 1);

	return 1;
}

