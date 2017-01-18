/* ParExp.cpp
 * Author: Zack GIll
 * ParExp is for CIS 452 Program 2. Evaluates an Arithmetic expression
 * using a tree and multiple-processes, and IPC using pipes. 
 */

#include <map>
#include <string>
#include <string.h>
#include <algorithm>
#include <vector>
#include <stack>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

using namespace std;

vector<string> tokenize(string &input);

bool isOperator(string& test);
bool isOperand(string& test);


bool immed;
pid_t mainProcess;

/* Sig Handlers */

/* sigCatch is used for the main Process to trigger the rest of the processes
 * by sending SIGUSR1 to all other processes it in's group. */
void sigCatch(int signum)
{
	if(getpid() == mainProcess)
	{
		kill(0, SIGUSR1);
	}
	return;
}

/* Handler for SIGUSR1 interrupt. Typically only sent by Main process. Once recived, all processes set their copy
 * of Global immed to true, then return. This allows those currently paused to continue and will have processes
 * that are not leaf nodes continue as soon as children are done.
 */
void userCatch(int signum)
{
	immed = true;
	return;
}

// Node for tree.
struct Node
{
	string data;
	Node* left;
	Node* right;
	bool full; // Meaning both left and right sub-tree cannot be added to.
};


// tree, called subTree, used to store the expression as a tree.
class subTree
{
public:
	int size;
	Node* root;
	subTree()
	{
		size = 0;
		root = NULL;
	}
	
	// Recursive Function to check (and set) the full flag on all nodes in tree.
	// Check if right is full, then if right is full. If both are full, then the current node is also
	// full, meaning both left and right subtrees cannot be added to.
	void checkFullHelper(Node* node)
	{
		if(isOperand(node->data))
			node->full = true;
		else{
			if(node->left == NULL)
				node->full = false;
			else
				checkFullHelper(node->left);
			if(node->right == NULL)
				node->full = false;
			else
				checkFullHelper(node->right);	
		}
		if(node->left != NULL && node->right != NULL && !node->full)
			node->full = node->left->full && node->right->full;
	}

	// Easy to call function that starts the recurisve check Full function
	void checkFull()
	{
		checkFullHelper(root);	
	}

	// Insert a new node into Tree.
	// Based on how the Order of Operations and Prefix syntax goes, insert logic is thus:
	// If possilbe, always insert left, either Directly or into the left subtree. If the left is full,
	// then you insert into the right subtree, trying again to go left once in there. If both left and right
	// are full, something went wrong, exit.
	// All the current->full = lines are there to make sure Nodes are marked as full when needed.
	void insert(string key){
		Node* temp = new Node();
		temp->data = key;
		if(isOperand(temp->data))
			temp->full = true;
		else
			temp->full = false;

		if(root == NULL){
			root = temp;
			size++;
		}

		else{
			Node* current = root;
			while(true)
			{
				checkFull();
				// Logic: If you can, insert to the left.

				if(current->left == NULL){
					current->left = temp;
					size++;
					if(current->right != NULL)
						current->full = current->left->full && current->right->full;
					break;
				}
				// If left is not null, see if left subtree has room
				else
				{
					if(current->left->full){
						if(current->right == NULL){
							current->right = temp;
							size++;
							current->full = current->left->full && current->right->full;
							break;
						}

						else{
							if(current->right->full){
								cout << "FULL TREE MAN\n";
								exit(1);
							}
							else{
								current->full = current->left->full && current->right->full;
								current = current->right;
							}		
						}
					}
					else{
						current->full = current->left->full && current->right->full;
						current = current->left;
					}
				}
			}
		}

	}

	// Cleaning up a tree with clear. It starts the recursive remove on the root.
	void clear(){
		remove(root);
	}

	// Recursive remove. Recursive call to remove left, then right, then delete self
	void remove(Node* node)
	{
		if(node == NULL)
			return;
		else{
			remove(node->left);
			remove(node->right);
		}
		delete node;
		size--;
	}

	// Call to start printTest, the recursive print function. Appends a new line to cout to flush buffer.
	void print(){
		printTest(root);
		cout << "\n";
	}

	// Recursive tree print. Traverse in manner to print out the prefix function, self then left then right.
	void printTest(Node* node)
	{
		if(node == NULL)
			return;
		cout << node->data + " ";
		printTest(node->left);
		printTest(node->right);
	}

};


// Function to return if string is an operator.
bool isOperator(string& test)
{
	if(test == "+" || test == "-" || test == "/" || test == "*")
		return true;
	return false;
}

// Function to return if string is an operand.
bool isOperand(string& test)
{
	try{
		stof(test, NULL);
	}
	catch(invalid_argument& e){
		return false;
	}
	return true;
}

// Custom made tokenize function, since there isn't a nice built in one for C++. Uses cstring.strtok()
// Returns tokens in a vector of strings.
vector<string> tokenize(string &input)
{
	char *tokens;
	const char *temp = input.c_str();
	string delim = " ";
	const char* test = delim.c_str();
	char *cstr = new char[input.length() + 1];
	strcpy(cstr, temp);
	tokens = strtok(cstr, test);
	vector<string> output;
	while(tokens != NULL)
	{
		output.push_back(tokens);
		tokens = strtok(NULL, test);
	}
	return output;
}

// Converts string Artihmetic Expression from Infix to Prefix notation.
// Uses following logic: 1. Reverse. 2. Convert to Post-fix. 3. Reverse.
string inToPrefix(string input)
{
	// Setting up output string, precdence map, and the stacks needed for conversion.
	string out = "";
	map<char, int> opPrec;
	opPrec['-'] = 0;
	opPrec['+'] = 0;
	opPrec['/'] = 1;
	opPrec['*'] = 1;

	stack<string> oper;

	// Step 1, reverse the string.
	reverse(input.begin(), input.end());

	// Get tokens of string.
	vector<string> tokens = tokenize(input);

	// Go through vector of string, tokens.
	// Step 2 of logic.
	for(string s: tokens)
	{
		// If is an Operand, append to output string
		if(isOperand(s)){
			out += s + " ";
		}
		
		// If it is an operator, pop operators from the stack of greater precdence than self. Append.
		// Push current operator token onto the stack.
		if(isOperator(s)){
			string popped;
			string temp;
			while(oper.size() > 0)
			{
				temp = oper.top();
				if(opPrec[temp[0]] > opPrec[s[0]])
				{
					out += temp + " ";
					oper.pop();
				}
				else
					break;
			}
			oper.push(s);
		}
	}
	// Once done going through the tokens, put the rest of the operators from stack onto output String
	while(oper.size() > 0)
	{
		string pop = oper.top();
		out += pop + " ";
		oper.pop();
	}

	// Step 3: Reverse output to get Pre-fix notation.
	reverse(out.begin(), out.end());
	return out;
}

// Helper function to create tree out of InFix Expression. Converts to Prefix, tokenizes, and calls insert
// for each token.
void buildTree(string& input, subTree* out)
{
	string temp = inToPrefix(input);
	vector<string> tokens = tokenize(temp);
	for(string s: tokens)
	{
		out->insert(s);
	}
}

// This is where most of the work is done. 
// This is a recursive function to evaluate the expression when stored as a tree.
// Pass a current Node and the write end of a pipe.
// Is only called on Nodes with operator data.
void simpleFork(Node* node, int writePipe)
{

	pid_t parent,child;
	parent = getpid();
	// Before Fork, copy parent id to parent. This way, both child and parent have copy.
	// If node is not an Operator, do not bother.
	if(isOperator(node->data))
	{
		int pip[2];
		pipe(pip);
		// If child, do work and fork
		if ((child = fork()) == 0)
		{
			cout << "Parent PID : " << parent << " is parent of: " << getpid() << "\n";

			// Close read end of pip
			close(pip[0]);
		

			// Output the operator this child is handling	
			cout << getpid() << " is handling " + node->data << "\n";
			// If both left and right are just operands, write to pip and exit
			bool left = isOperand(node->left->data);
			bool right = isOperand(node->right->data);
			float toWrite;
			// Case to determine what operation to do.
			if(left && right){
				switch (node->data[0]){
				case '+': 
					toWrite = stof(node->left->data, NULL) + stof(node->right->data, NULL);
					break;
				
				case '-': 
					toWrite = stof(node->left->data, NULL) - stof(node->right->data, NULL);
					break;
		
				case '*': 
					toWrite = stof(node->left->data, NULL) * stof(node->right->data, NULL);
					break;

				case '/': 
					toWrite = stof(node->left->data, NULL) / stof(node->right->data, NULL);
					break;

				}
				// If not immediate, wait for signal. Immed might change based on previous signals
				// if this current processes is the lowest subtree.
				if(!immed){
					pause();
				}

				// Output return and where returning to
				cout << "Child " << getpid() << " returning: " << toWrite << " to parent: " << parent << "\n";
				int error = write(pip[1], &toWrite, sizeof(toWrite));
				if(error == -1)
				{
					perror("Error writing to pipes\n");
					exit(1);
				}
				exit(0);
			}
			float leftFloat, rightFloat;
			// Both need new process, call this function again with left and right nodes.
			if(!left && !right)
			{
				// Create a pipe for both right and left subtree
				// Call this function recursive on left, then right subtree.
				// Because calling left first, then right, a process might not show up
				// using ps uef when expected. The correct number of processes will be spawned,
				// but since left subtree is dealt with first, and blocks, the right will not be
				// spawned till after the left goes.
				int leftPipe[2];
				pipe(leftPipe);
				simpleFork(node->left, leftPipe[1]);
				int rightPipe[2];
				pipe(rightPipe);
				simpleFork(node->right, rightPipe[1]);
				
				int errorLeft = read(leftPipe[0], &leftFloat, sizeof(leftFloat));
				if(errorLeft == -1){
					perror("Error Reading\n");
					exit(1);
				}
				close(leftPipe[0]);
				close(leftPipe[1]);
				int errorRight = read(rightPipe[0], &rightFloat, sizeof(rightFloat));
				if (errorRight == -1){
					perror("Error Reading pipe\n");
					exit(1);
				}
				
				close(rightPipe[0]);
				close(rightPipe[1]);
			}
			// Only the right subtree needs to do more work
			else if(!right){
				// Set the value of leftFloat
				leftFloat = stof(node->left->data, NULL);
				
				// Create pipe 
				int rightPipe[2];
				pipe(rightPipe);
				
				simpleFork(node->right, rightPipe[1]);
				int error = read(rightPipe[0], &rightFloat, sizeof(rightFloat));
				if (error == -1){
					perror("Error Reading pipe\n");
					exit(1);
				}
				// Close pipe when done. Did not close
				// write end to make sure it was open when
				// recursive call forks.
				close(rightPipe[0]);
				close(rightPipe[1]);
			}
			// Only left subtree needs to do more work
			else if(!left){
				// Set value of rightFloat to right node data.
				rightFloat = stof(node->right->data, NULL);

				// Setup the pipe to read from
				int leftPipe[2];
				pipe(leftPipe);
				
				// Recursive call to the left subtrees.
				simpleFork(node->left, leftPipe[1]);
				int error = read(leftPipe[0], &leftFloat, sizeof(leftFloat));
				if (error == -1){
					perror("Error Reading pipe\n");
					exit(1);
				}
				// Close pipe when done. Did not close write
				// end earlier to avoid closing it before forked
				// process had a chance to use it.
				close(leftPipe[0]);
				close(leftPipe[1]);
			}
			// Once the float variables are determined, perform
			// operation on them.
			switch (node->data[0]){
			case '+': 
				toWrite = leftFloat + rightFloat;
				break;
			
			case '-': 
				toWrite = leftFloat - rightFloat;
				break;
	
			case '*': 
				toWrite = leftFloat * rightFloat;
				break;

			case '/': 
				toWrite = leftFloat / rightFloat;
				break;

				}
			// If not immediate, wait for signal. See above pause
			// for more information.
			if(!immed){
				pause();
			}
			// Print out the return value.
			cout << "Child " << getpid() << " returning: " << toWrite << " to parent: " << parent << "\n";
			int error = write(pip[1], &toWrite, sizeof(toWrite));
			if(error == -1){
				perror("error writing\n");
				exit(1);
			}
			
			exit(0);

		}
		else {
			//Reading only, close write end
			close(pip[1]);

			// Read from child
			float value;
			int error = read(pip[0], &value, sizeof(value));
			if(error == -1)
			{
				perror("error reading\n");
				exit(1);
			}	
			// Write to pipe provided to function
			write(writePipe, &value, sizeof(value));

			// Wait for child to exit
			int status;
			waitpid(child, &status, 0);
			if(status < 0){
				cout << "ERROR in child\n";
			}
		}
	}

}

// Evaluate function. Calls simple fork after generating a tree.
float evaluate(const char* infix_expr, bool immediate)
{
	// Setting the main process and outputting it.
	mainProcess = getpid();
	cout << "Main Process PID: " << mainProcess << "\n";
	
	// Setting up the SigHandlers for the "main" process
	signal(SIGINT, sigCatch);
	signal(SIGUSR1, userCatch);
	// Setup output 
	float out;

	// Create a c++ string from the char* provided.
	string temp(infix_expr);

	// Output the provided string
	cout << "In-fix: " << temp << "\n";
	// Create a tree.
	subTree* tree = new subTree();
	buildTree(temp, tree);
	// Output the tree
	cout << "Prefix Tree: ";
	tree->print();
	
	// Set global immediate value equal to passed value.
	immed = immediate;

	// Instead of writing new function, just using a "fake" pipe and calling simpleFork on root
	// Fake pipe since it is talking to itself
	int selfPipe[2];
	pipe(selfPipe);

	// Start simpleFork, passing the root and the write end of the pipe.
	simpleFork(tree->root, selfPipe[1]);	

	// After done with function, get output from pipe.
	read(selfPipe[0], &out, sizeof(out));

	// Close pipe, clear tree.
	close(selfPipe[0]);
	close(selfPipe[1]);
	tree->clear();
	delete tree;
	return out;
}


int main()
{

	
printf("Test  1 ===> %7.3f\n", evaluate("20.0 - 19.0", false));


/*
	mainProcess = getpid();
	cout << "Main process " << mainProcess << "\n";
	
	float out = evaluate("2.0 * 3.0 - 9.0 + 7.58 / 82.0 + 69.3 - 87.0 + 88.0 * 9654.0 + 666.0 / 88.0 * 88.0 - 99.0 / 88.0 + 55.0 + 66.0 - 22.0 * 88.0 + 65.0 + 20.0 - 78.0 + 22.0 + 23.0 + 24.0 - 25.0 * 26.0 / 27.0 / 28.0 * 29.0 + 30.0", false);
	float real = -852722.451301;
	cout << out << "\n";
	cout << real << "\n\n";


	out = evaluate("2 + 3 + 4 + 5", false);
	cout << out << " Should be : " << 2 + 3 + 4 + 5 <<"\n\n";
	

	out = evaluate("2.0 * 3.0 + 4.0 / 5.0", false);
	cout << out << " Should be : " << 2.0 * 3.0 + 4.0 / 5.0 << "\n\n";


	out = evaluate("2.0 * 3.0 + 4.0", true);
	cout << out << " Should be : " << 2.0 * 3.0 + 4.0 << "\n\n";

	out = evaluate("2.0 + 3.0 * 4.0", false);
	cout << out << " Should be " << 2.0 + 3.0 * 4.0 <<"\n\n";

	out = evaluate("2 + 3 * 4 - 5 / 6", false);
	cout << out << " Should be " << 2.0 + 3.0 * 4.0 - 5.0 / 6.0 << "\n\n";

	out = evaluate("11.0 - 10.0", false);
	cout << out << " Should be " << 11.0 - 10.0 << "\n\n";

	out = evaluate("19.0 - 11.0", false);
	cout << out << " Should be " << 19.0 - 11.0 << "\n\n";

	out = evaluate("5.0 + 4.0", false);
	cout << out << " Should be " << 5.0 + 4.0 << "\n\n";

	out = evaluate("10.0 - 0.0", false);
	cout << out << " Should be " << 10.0 - 0.0 << "\n\n";
	
*/
	return 0;
}
