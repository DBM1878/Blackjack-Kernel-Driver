#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/random.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

MODULE_AUTHOR("David Middour");
MODULE_LICENSE("GPL");

void randomizeDeck(void);
void resetGame(void);
void dealCards(void);
void calcScore(void);
void checkScores(void);
int checkAces(int player);
void playerHit(void);
void dealerTurn(void);
int compareScores(void);
ssize_t printHands(char*, size_t);
//char printCards(void);

#define DEVICE_NAME "blackjack"
static int doThing; //variable to prevent infinite loop when reading from module
static int gameState; //variable tracking game state
static int playerScore = 0; //variable keeping track of player score
static int dealerScore = 0; //variable keeping track of dealer score
static int msgCode = -1; //variable indicating special responses from game, player win/lose, dealer win/lose, invalid input
//list of special game responses
static const char *gameMsg[] = {"Invalid reseponse: must 'Reset' game first\n", 
						"Invalid response: must 'Shuffle' deck before deal\n",
						"Invalid response: must 'Deal' before 'Hit' or 'Stay'\n",
						"Player has score of 21, Player Wins!\n",
						"Players score went over 21, Player Loses, Dealer Wins!\n",
						"Dealer has score of 21, Dealer Wins!\n",
						"Dealers score went over 21, Dealer Loses, Player Wins\n",
						"Players score is greater than Dealers score, Plyaer Wins!\n",
						"Dealers score is greater than Players score, Dealer Wins!\n",
						"Invalid Message: Expecting 'Reset'\n",
						"Invalid Message: Expecting 'Shuffle'\n",
						"Invalid Message: Expecting 'Deal'\n",
						"Invalid Message: Expecting 'Hit' or 'Stay'\n",
						"Game has Ended, to play again please Reset\n",
						"Dealer and Player have tied!\n"
						};
//varibale holding deck of cards
static char *deck[] = {"2H", "3H", "4H", "5H", "6H", "7H", "8H", "9H", "10H", "JH", "QH", "KH", "AH",
								"2S", "3S", "4S", "5S", "6S", "7S", "8S", "9S", "10S", "JS", "QS", "KS", "AS",
								"2C", "3C", "4C", "5C", "6C", "7C", "8C", "9C", "10C", "JC", "QC", "KC", "AC",
								"2D", "3D", "4D", "5D", "6D", "7D", "8D", "9D", "10D", "JD", "QD", "KD", "AD"
							};
static int deckPos = 0; //variable holding position in deck
static char *dealerHand[12]; //variable holding dealer cards
static int dealerNumCards; //variable counting number of cards in dealers hand
static int dealerNumAces = 0; //variable keeping track of number of aces because they're special
static char *playerHand[12]; // variable holding player cards
static int playerNumCards; //variable counting number of cards in players hand
static int playerNumAces = 0; //variable keeping track of number of aces because they're special


//reads from module - prints game response to console
static ssize_t blackjack_read(struct file *file, char *buffer, size_t len, loff_t *offset) {
	//variable that prevents an infinite loop
	if(doThing == 0) {
		return 0;
	}

	//special game state has occured, either player or dealer has reached or exceeded 21, or player inputed invalid command
	if(msgCode > -1) {
		len = printHands(buffer, len);
		int response = msgCode;
		msgCode = -1;
		if(copy_to_user((buffer+len), gameMsg[response], strlen(gameMsg[response])+1) != 0) {
			printk(KERN_ERR);
			return -EFAULT;
		}
		len += strlen(gameMsg[response])+1;
		if(response > 2 && response < 9) {
			gameState = 5;
		}
		return len;
	}

	//start of game
	//only gets printed if reset is not the first command entered when module is loaded
	if(gameState == 0) {
		doThing = 0;
		const char *rules = "How to play\nCommand: What it does\nRESET: Reset game state\nSHUFFLE: Shuffle the deck\nDEAL: Deal cards to dealer and player\nHIT: Player gets dealt a card\nSTAY: Player ends turn\n\nGame will deal cards and display to console\nGame will ask if Player wants to be dealt another card - Player responds 'hit' or 'stay'\n\nGood Luck, may the odds be in your favor\n";
		if(copy_to_user(buffer, rules, strlen(rules)+1) != 0) {
			printk(KERN_ERR);
			return -EFAULT;
		}
		return strlen(rules)+1;
	}

	//game has been reset and is ready to shuffle
	if(gameState == 1) {
		doThing = 0;
		static const char resetMsg[] = "Game Reset, ready to Shuffle deck\n\n";
		if(copy_to_user(buffer, resetMsg, strlen(resetMsg+1)) != 0) {
				printk(KERN_ERR);
			return -EFAULT;
		}
		return strlen(resetMsg);
	}
	
	//deck has been shuffled, ready to deal
	if(gameState == 2) {
		doThing = 0;
		static const char shuffleMsg[] = "Deck Shuffled, ready to Deal\n";
		if(copy_to_user((buffer), shuffleMsg, strlen(shuffleMsg)+1) != 0) {
				printk(KERN_ERR);
			return -EFAULT;
		}
		return strlen(shuffleMsg);
	}

	//game state where player and dealer take turns, will print out player and dealer hands
	if(gameState == 3) {
		doThing = 0;
		len = printHands(buffer, len);
		return len;
	}

	//state after both player and dealer turns are taken.
	//checks for who the winner is and displays to console
	if(gameState == 4) {
		doThing = 0;
		len = printHands(buffer, len);
		int winner = compareScores();
		if(winner == 2) {
			//player wins
			if(copy_to_user(buffer+len, gameMsg[7], strlen(gameMsg[7])+1) != 0) {
				printk(KERN_ERR);
				return -EFAULT;
			}
			len += strlen(gameMsg[7])+1;
		} else if(winner == 0) {
			//dealer wins
			if(copy_to_user(buffer+len, gameMsg[8], strlen(gameMsg[8])+1) != 0) {
				printk(KERN_ERR);
				return -EFAULT;
			}
			len += strlen(gameMsg[8])+1;
		} else {
			//its a tie
			if(copy_to_user(buffer+len, gameMsg[14], strlen(gameMsg[14])+1) != 0) {
				printk(KERN_ERR);
				return -EFAULT;
			}
			len += strlen(gameMsg[14])+1;
		}
		gameState = 5;
		return len;
	}
	
	doThing = 0;

	return 0;
}

//runs when the module is opened, regulates how many times read function is called
static int blackjack_open(struct inode *inode, struct file *file) {
	//module gets opened, sets doThing so events like printing to console occur only once
	doThing = 1;
	return 0;
}

//takes inputed command from user and does the appropriate action in response
static ssize_t blackjack_write(struct file *file, const char *buffer, size_t len, loff_t *offset) {
	char command[256];
	//if length of input is too long set it to max value
	if(len > 255) {
		len = 255;
	}
	//get inputed command from user
	if(copy_from_user(command, buffer, len) != 0) {
		printk(KERN_ERR);
		return -EFAULT;
	}

	if(!strncmp(command, "RESET", 5) || !strncmp(command, "reset", 5) || !strncmp(command, "Reset", 5)) {
		//Reset game
		resetGame();
	} else if(!strncmp(command, "SHUFFLE", 7) || !strncmp(command, "shuffle", 7) || !strncmp(command, "Shuffle", 7)) {
		//Shuffle
		if(gameState == 5) {
			msgCode = 13;
			return len;
		} else if(gameState != 1) {
			msgCode = 0;
			return len;
		}
		randomizeDeck(); //randomize deck
		gameState = 2; //set game state
	} else if(!strncmp(command, "DEAL", 4) || !strncmp(command, "Deal", 4) || !strncmp(command, "deal", 4)) {
		//Deal out cards to dealer and player
		if(gameState == 5) {
			msgCode = 13;
			return len;
		} else if(gameState != 2) {
			msgCode = 1;
			return len;
		}
		dealCards();
		gameState = 3;
	} else if(!strncmp(command, "HIT", 3) || !strncmp(command, "Hit", 3) || !strncmp(command, "hit", 3)) {
		//player chooses to be dealt another card
		if(gameState == 5) {
			msgCode = 13;
			return len;
		} else if(gameState != 3) {
			msgCode = 2;
			return len;
		}
		playerHit();
	} else if(!strncmp(command, "STAY", 4) || !strncmp(command, "Stay", 4) || !strncmp(command, "stay", 4)) {
		//player chooses to stay at current number of cards
		if(gameState == 5) {
			msgCode = 13;
			return len;
		} else if(gameState != 3) {
			msgCode = 2;
			return len;
		}
		dealerTurn();
		gameState = 4;
	} else {
		//if invalid unknown command is given, respond appropriately based on game state
		if(gameState == 0) {
			msgCode = 9;
		} else if(gameState == 1) {
			msgCode = 10;
		} else if(gameState == 2) {
			msgCode = 11;
		} else if(gameState == 3) {
			msgCode = 12;
		} else if(gameState == 5) {
			msgCode = 13;
		}
	}
    return len;
}

//reset the game state and player and dealer scores
void resetGame(void) {
	gameState = 1; //set game state
	playerScore = 0; //reset player score
	dealerScore = 0; //reset dealer score
	deckPos = 0; //reset deck position
}

//shuffle the deck
void randomizeDeck(void) {
	unsigned int randCell1; //1st random location in deck
	unsigned int randCell2; //2nd random location in deck
	char *temp; // temp value

	//repeat 1000 times to randomize deck
	int i = 0;
	for(i; i < 1000; i++) {
		//get two random locations in deck
		get_random_bytes(&randCell1, sizeof(randCell1));
		randCell1 %= 52;
		get_random_bytes(&randCell2, sizeof(randCell2));
		randCell2 %= 52;

		//swap locations in deck
		temp = deck[randCell1];
		deck[randCell1] = deck[randCell2];
		deck[randCell2] = temp;
	}
}

//deal cards to the player and the dealer, then check scores of both
void dealCards(void) {
	playerHand[0] = deck[0]; //player dealt
	dealerHand[0] = deck[1]; //dealer dealt
	playerHand[1] = deck[2]; //player dealt
	dealerHand[1] = deck[3]; //dealer dealt
	deckPos = 4;
	dealerNumCards = 2;
	playerNumCards = 2;
	calcScore();
	checkScores();
}

//calculate the players and dealers scores
void calcScore(void) {
	//reset score and recalculaate
	dealerScore = 0;
	playerScore = 0;
	int i = 0;
	//calc dealer score
	for(i; i < dealerNumCards; i++) {
		if(dealerHand[i][0] == 'J' || dealerHand[i][0] == 'Q' || dealerHand[i][0] == 'K') {
			//jacks, queens and kings are worth 10
			dealerScore += 10;
		} else if(dealerHand[i][0] == 'A') {
			//aces are worth 1 or 11, if worth 11, increment ace counter
			if(dealerScore + 11 > 21) {
				dealerScore += 1;
			} else {
				dealerNumAces += 1;
				dealerScore += 11;
			}
		} else {
			//number card, increment score
			if(strlen(dealerHand[i]) == 3) {
				dealerScore += 10;
			} else {
				dealerScore += (dealerHand[i][0] - 48);
			}
		}
	}

	//calc player score
	i = 0;
	for(i; i < playerNumCards; i++) {
		if(playerHand[i][0] == 'J' || playerHand[i][0] == 'Q' || playerHand[i][0] == 'K') {
			//jacks, queens and kings are worth 10
			playerScore += 10;
		} else if(playerHand[i][0] == 'A') {
			//aces are worth 1 or 11, if worth 11, increment ace counter
			if(playerScore + 11 > 21) {
				playerScore += 1;
			} else {
				playerNumAces += 1;
				playerScore += 11;
			}
		} else {
			//number card, increment score
			if(strlen(playerHand[i]) == 3) {
				playerScore += 10;
			} else {
				playerScore += (playerHand[i][0] - 48);
			}		
		}
	}
}

//check scores when dealing to see if win or bust state is reached
void checkScores(void) {
	if(playerScore == 21) {
		//player wins
		msgCode = 3;
	} else if(playerScore > 21) {
		//check the value of player aces
		if(checkAces(0) || playerNumAces == 0) {
			//player over 21
			msgCode = 4;
		}
	} else if(dealerScore == 21) {
		//dealer wins
		msgCode = 5;
	} else if(dealerScore > 21) {
		//check value of dealer aces
		if(checkAces(1) || dealerNumAces == 0) {
			//dealer over 21
			msgCode = 6;
		}
	}
}

//check value of aces and determine if they are worth 1 or 11
int checkAces(int player) {
	if(player) {
		//dealer
		int i = 0;
		for(i; i < dealerNumCards; i++) {
			if(dealerScore > 21 && dealerNumAces > 0) {
				//only adjust ace if score is over 21 and there is at least one ace that has not been adjusted
				if(dealerHand[i][0] == 'A') {
					//adjust ace value and decrement ace counter
					dealerScore -= 11;
					dealerScore += 1;
					dealerNumAces -= 1;
				}
			}
		}
		if(dealerScore > 21) {
			//if score is still over 21, bust
			return 1;
		} else {
			//score under 21, continue playing
			return 0;
		}
	} else {
		//player
		int i = 0;
		for(i; i < playerNumCards; i++) {
			if(playerScore > 21 && playerNumAces > 0) {
				//only adjust ace if score is over 21 and there is at least one ace that has not been adjusted
				if(playerHand[i][0] == 'A') {
					//adjust ace value and decrement ace counter
					playerScore -= 11;
					playerScore += 1;
					playerNumAces -= 1;
				}
			}
		}
		if(playerScore > 21) {
			//if score is still over 21, bust
			return 1;
		} else {
			//score under 21, continue playing
			return 0;
		}
	}
}

//player gets dealt a card and score gets calculated and checked
void playerHit(void) {
	playerHand[playerNumCards] = deck[deckPos];
	playerNumCards++;
	deckPos++;
	calcScore();
	checkScores();
}

//dealers turn. gets dealt cards until over 17, win or bust
void dealerTurn(void) {
	while(dealerScore < 17) {
		dealerHand[dealerNumCards] = deck[deckPos];
		dealerNumCards++;
		deckPos++;
		calcScore();
		checkScores();
	}
}

//compares scores of dealer and player
int compareScores(void) {
	if(dealerScore > playerScore) {
		return 0;
	} else if(dealerScore == playerScore) {
		return 1;
	} else {
		return 2;
	}
}

//prints out player and dealer hands
ssize_t printHands(char* buffer, size_t len) {
	//creates messages
	static char dealMsg[25] = "Dealer Cards, Score: ";
	static char playerMsg[25] = "Player Cards, Score: ";
	char playerMove[] = "\nPlayer: Hit or Stay\n";

	//updates the dealers message with its current score
	char *dealerScoreString;
	char *dealerScoreString2;
	if(dealerScore / 10 > 0) {
		dealerScoreString = (dealerScore / 10) + '0';
		dealerScoreString2 = (dealerScore % 10) + '0';
		dealMsg[21] = dealerScoreString;
		dealMsg[22] = dealerScoreString2;
	} else {
		dealerScoreString = dealerScore + '0';
		dealMsg[21] = dealerScoreString;
		dealMsg[22] = ' ';
	}
	dealMsg[23] = '\n';

	//updates the players message with their current score
	char *playerScoreString;
	char *playerScoreString2;
	if(playerScore / 10 > 0) {
		playerScoreString = (playerScore / 10) + '0';
		playerScoreString2 = (playerScore % 10) + '0';
		playerMsg[21] = playerScoreString;
		playerMsg[22] = playerScoreString2;
	} else {
		playerScoreString = playerScore + '0';
		playerMsg[21] = playerScoreString;
		playerMsg[22] = ' ';
	}
	playerMsg[23] = '\n';

	//puts dealer message into buffer
	if(copy_to_user((buffer), dealMsg, strlen(dealMsg)+1) != 0) {
			printk(KERN_ERR);
		return -EFAULT;
	}
	len = strlen(dealMsg);

	//puts dealers hand into the buffer
	int i = 0;
	for(i; i < dealerNumCards; i++) {
		if(copy_to_user((buffer+len), dealerHand[i], strlen(dealerHand[i])+1) != 0) {
			printk(KERN_ERR);
			return -EFAULT;
		}
		len += strlen(dealerHand[i]);
		if(i != dealerNumCards-1) {
			if(copy_to_user((buffer+len), ", ", strlen(", ")+1) != 0) {
				printk(KERN_ERR);
				return -EFAULT;
			}
			len += strlen(", ");
		} else {
			if(copy_to_user((buffer+len), "\n", strlen("\n")+1) != 0) {
				printk(KERN_ERR);
				return -EFAULT;
			}
			len += strlen("\n");
		}
	}

	if(copy_to_user((buffer+len), "\n\n", strlen("\n\n")+1) != 0) {
			printk(KERN_ERR);
		return -EFAULT;
	}
	len += strlen("\n\n");

	//puts players hand into the buffer
	i = 0;
	for(i; i < playerNumCards; i++) {
		if(copy_to_user((buffer+len), playerHand[i], strlen(playerHand[i])+1) != 0) {
			printk(KERN_ERR);
			return -EFAULT;
		}
		len += strlen(playerHand[i]);
		if(i != playerNumCards-1) {
			if(copy_to_user((buffer+len), ", ", strlen(", ")+1) != 0) {
				printk(KERN_ERR);
				return -EFAULT;
			}
			len += strlen(", ");
		} else {
			if(copy_to_user((buffer+len), "\n", strlen("\n")+1) != 0) {
				printk(KERN_ERR);
				return -EFAULT;
			}
			len += strlen("\n");
		}
	}

	//puts player message into the buffer
	if(copy_to_user((buffer+len), playerMsg, strlen(playerMsg)+1) != 0) {
			printk(KERN_ERR);
		return -EFAULT;
	}
	len += strlen(playerMsg);

	if(gameState != 4) {
		//message that asks if the player wants to 'Hit' or 'Stay'
		if(copy_to_user((buffer+len), playerMove, strlen(playerMove)+1) != 0) {
				printk(KERN_ERR);
			return -EFAULT;
		}
		len += strlen(playerMove);
	}

	return len;
}
/*
char printCards(void) {
	char *cardTop = "[%d     ]\n";
	char *cardMid = "| %s |\n";
	char *cardLow = "[     %d]\n";

	int dealerCardSize = strlen(cardTop)*dealerNumCards + strlen(cardMid)*dealerNumCards + strlen(cardLow)*dealerNumCards;
	char *dealerCards[dealerCardSize];
	char *dealerNums[12];
	char *dealerSuits[12];
	int i = 0;
	for(i; i < dealerNumCards; i++) {
		if(sizeof(dealerHand[i]-1 == 3)) {
			dealerNums[i] = dealerHand[i][0];
			strcat(dealerNums[i], dealerHand[i][1]);
		} else {
			dealerNums[i] = dealerHand[i][0];
		}
		dealerSuits[i] = dealerHand[i][sizeof(dealerHand[0])-1];
	}
	i = 0;
	for(i; i < dealerNumCards; i++) {
	}

	char *playerCards = "[%d     ]\n| %s |\n[     %d]\n";
	char *playerNums[12];
	char *playerSuits[12];
	i = 0;
	for(i; i < playerNumCards; i++) {
		if(sizeof(playerHand[i]-1 == 3)) {
			playerNums[i] = playerHand[i][0];
			strcat(playerNums[i], playerHand[i][1]);
		} else {
			playerNums[i] = playerHand[i][0];
		}
		playerSuits[i] = playerHand[i][sizeof(playerHand[0])-1];
	}

	char *printCards[20];
	//strcat(printCards, dealerCards, dealerNums[0], dealer)
	return printCards;
}
*/


static const struct file_operations card_fops = {
	//defined file operations
    .read = blackjack_read,
	.open = blackjack_open,
	.write = blackjack_write,
};

static struct miscdevice card_device = {
	// Miscellaneous device structure
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &card_fops,
    .mode = 0666,
};

static int __init blackjack_init(void) {
	// Module initialization and registration
    int ret = misc_register(&card_device);
    if (ret) {
        //pr_err("Failed to register misc device: %d\n", ret);
		printk(KERN_ERR "Failed to register misc device: %d\n", ret);
        return ret;
    }

    //pr_info("Misc device registered successfully\n");
	printk(KERN_INFO "Device registered successfully\n");
	gameState = 0;
    return 0;
}

static void __exit blackjack_exit(void) {
	// Module cleanup
    misc_deregister(&card_device);
    printk(KERN_INFO"Misc device unregistered\n");
}

module_init(blackjack_init);
module_exit(blackjack_exit);
