 This utility will compute all permutations of just ONE word of each group, creating
all possible passwords from min-pw-sz to max-pw-size, optatively using all the possible
separators between permutated words.
 It can also add case-modified versions of each word to each group.

Usage:

group_permutator [options] group_of_words_file [separators_file]

options

	--max-word-sz=XX   Maximum word size, in characters, default: 64
	--max-group-sz=XX  Maximum word group size, in characters, default: 4096
	--max-sep-sz=XX    Maximum separator size, in characters, default: 64
	--min-pw-sz=XX     Minimum password length to generate, in characters, default: 8
	--max-pw-sz=XX     Maximum password length to generate, in characters, default: 16
	--max-depth=XX     Maximum count of groups to be used for each password, default: 64
	--uc               Also try the same words in uppercase, default: do not
	--lc               Also try the same words in lowercase, default: do not
	--cc               Also try the same words capitalized,  default: do not
	--tc               Also try the same words with toggled case,  default: do not
	
group_of_words_file is a file with groups of words, one group per line, words separated by spaces
separators_file     is an optative file with word separators to use, one separator for each line
	
 Please, note that spaces or tabs are NOT stripped from the separators file, to allow to include 
empty separators, or spaces in separators.
 Also, the group_of_words_file can contain duplicated words groups, if you want an specific group
of words to appear more than once in the same password
