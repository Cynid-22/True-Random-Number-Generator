# Passphrase Word Lists

The Passphrase output picks every word uniformly at random from one of these lists. Each list is
embedded in TRNG.exe when it is built. NordPass is the default.

When the app starts it counts the words in each embedded list and computes the bits per word as
log2(word count), so the numbers shown in the app always match the list that was built in. The
numbers below were measured from the files in this folder.

| List | File | Words | Bits per word | Word length | Characters | No word starts another |
|---|---|---|---|---|---|---|
| NordPass (default) | `NordPass_wordlist.txt` | 123,565 | 16.9149 | 3-10 | a-z | No |
| EFF Large | `eff_large_wordlist.txt` | 7,776 | 12.9248 | 3-9 | a-z, - | Yes |
| Diceware 8k | `diceware8k.txt` | 8,192 | 13.0000 | 1-6 | a-z, 0-9, ! " # $ % & ' ( ) * + - . : ; = ? @ | No |
| BIP39 English | `BIP39-English.txt` | 2,048 | 11.0000 | 3-8 | a-z | No |

Words needed for a given strength (bits per passphrase = words x bits per word):

| List | 64 bits | 80 bits | 100 bits | 128 bits | 6 words |
|---|---|---|---|---|---|
| NordPass | 4 | 5 | 6 | 8 | 101.5 bits |
| EFF Large | 5 | 7 | 8 | 10 | 77.5 bits |
| Diceware 8k | 5 | 7 | 8 | 10 | 78.0 bits |
| BIP39 English | 6 | 8 | 10 | 12 | 66.0 bits |

## NordPass

- Large English dictionary, lowercase letters only.
- The most bits per word, so the shortest passphrase for a given strength.
- Many words are rare, so passphrases are harder to remember and type.
- Some words start with another word (for example "aardvark" and "aardvarks"). Keep a separator
  that isn't a letter; with no separator, some passphrases can be split into words in more than
  one way and hold fewer bits than shown.

## EFF Large

- The Electronic Frontier Foundation's long list for passphrases made with five dice.
- Each line is a five-dice roll and a word (`11111<tab>abacus`); the app reads the word and ignores
  the roll.
- Common, memorable words.
- No word starts with another word, so a passphrase can only be read one way with any separator,
  or with none.
- Four words contain a hyphen: drop-down, felt-tip, t-shirt, yo-yo.

## Diceware 8k

- Diceware list sized for computer generation: 8,192 = 2^13 entries, exactly 13 bits per word.
- Very short entries; 702 contain digits and 79 contain symbols (for example a2, a&p, ain't, at&t).
- Compact passphrases, but harder to remember.
- One entry is "-" itself, the default separator, so the app shows the separator warning with
  the default settings. Use a space as the separator with this list.

## BIP39 English

- The English word list of Bitcoin's BIP39 standard.
- Short common words; the first 4 letters identify each word.
- Not a wallet recovery phrase: in BIP39 the last word carries checksum bits, which this generator
  doesn't add, so wallets will usually reject the result.

## Separator warning

The Passphrase options show an orange warning when both of these hold:

- some word in the selected list starts with another word, and
- the separator is empty or uses a character that also appears in the list's words.

With the current lists this means no separator with NordPass, Diceware 8k or BIP39 English, and,
with Diceware 8k, any separator that uses a letter, a digit or one of its symbols, including the
default "-". EFF Large never shows it. The check is cautious: the warning means a passphrase may be
readable in more than one way, not that it is. For example, in BIP39 English with no separator,
"canalarm" is both "can alarm" and "canal arm".

## Numbers

The Numbers field adds random digits 0-9 at the start or end of words, at most one before and one
after each word (so 0 to 2 x words). Each digit adds log2(10) = 3.32 bits, and the choice of spots
adds log2(C(2 x words, numbers)) bits. With Diceware 8k, whose entries already contain digits, an
added digit may look like part of a word, and the app shows a warning.

## File format

- One word per line, UTF-8 or ASCII, LF or CRLF line ends. Blank lines are skipped.
- Lists that start each line with a dice roll (EFF Large) are read as "roll, whitespace, word".
- A list is rejected, and Passphrase generation blocked, if a line holds more than one word, a word
  appears twice, or the list has fewer than 2 or more than 16,777,216 words.
