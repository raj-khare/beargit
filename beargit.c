#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>

#include "beargit.h"
#include "util.h"

/* Implementation Notes:
 *
 * - Functions return 0 if successful, 1 if there is an error.
 * - FS helper functions:
 *   * fs_mkdir(dirname): create directory <dirname>
 *   * fs_rm(filename): delete file <filename>
 *   * fs_mv(src,dst): move file <src> to <dst>, overwriting <dst> if it exists
 *   * fs_cp(src,dst): copy file <src> to <dst>, overwriting <dst> if it exists
 *   * write_string_to_file(filename,str): write <str> to filename (overwriting contents)
 *   * read_string_from_file(filename,str,size): read a string of at most <size> (incl.
 *     NULL character) from file <filename> and store it into <str>. Note that <str>
 *     needs to be large enough to hold that string.
 */

/* beargit init
 *
 * - Create .beargit directory
 * - Create empty .beargit/.index file
 * - Create .beargit/.prev file containing 0..0 commit id
 *
 * Output (to stdout):
 * - None if successful
 */

int beargit_init(void)
{
  fs_mkdir(".beargit");

  FILE *findex = fopen(".beargit/.index", "w");
  fclose(findex);

  FILE *fbranches = fopen(".beargit/.branches", "w");
  fprintf(fbranches, "%s\n", "master");
  fclose(fbranches);

  write_string_to_file(".beargit/.prev", "0000000000000000000000000000000000000000");
  write_string_to_file(".beargit/.current_branch", "master");

  return 0;
}

/* beargit add <filename>
 * 
 * - Append filename to list in .beargit/.index if it isn't in there yet
 *
 * Possible errors (to stderr):
 * >> ERROR: File <filename> already added
 *
 * Output (to stdout):
 * - None if successful
 */

int beargit_add(const char *filename)
{
  FILE *findex = fopen(".beargit/.index", "r");
  FILE *fnewindex = fopen(".beargit/.newindex", "w");

  char line[FILENAME_SIZE];
  while (fgets(line, sizeof(line), findex))
  {
    strtok(line, "\n");
    if (strcmp(line, filename) == 0)
    {
      fprintf(stderr, "ERROR: File %s already added\n", filename);
      fclose(findex);
      fclose(fnewindex);
      fs_rm(".beargit/.newindex");
      return 3;
    }

    fprintf(fnewindex, "%s\n", line);
  }

  fprintf(fnewindex, "%s\n", filename);
  fclose(findex);
  fclose(fnewindex);

  fs_mv(".beargit/.newindex", ".beargit/.index");

  return 0;
}

/* beargit rm <filename> */

int beargit_rm(const char *filename)
{
  FILE *findex = fopen(".beargit/.index", "r");
  FILE *fnewindex = fopen(".beargit/.newindex", "w");
  int found = 0;
  char line[FILENAME_SIZE];
  while (fgets(line, sizeof(line), findex))
  {
    strtok(line, "\n");
    if (strcmp(line, filename) == 0)
      found = 1;
    else
      fprintf(fnewindex, "%s\n", line);
  }
  fclose(findex);
  fclose(fnewindex);

  if (!found)
  {
    fprintf(stderr, "ERROR: File %s not tracked\n", filename);
    fs_rm(".beargit/.newindex");
    return 1;
  }
  fs_mv(".beargit/.newindex", ".beargit/.index");
  return 0;
}

/* beargit commit -m <msg> */

const char *go_bears = "GO BEARS!";

int is_commit_msg_ok(const char *msg)
{
  int i = 0, j = 0;
  while (msg[i] != '\0')
  {
    if (msg[i++] == go_bears[j++])
    {
      if (go_bears[j] == '\0')
        return 1;
    }
    else
      j = 0;
  }
  return 0;
}

void next_commit_id_hw1(char *commit_id)
{
  int i = 0;

  while (commit_id[i] != '\0')
  {
    switch (commit_id[i])
    {
    case '0':
    case 'c':
      commit_id[i] = '6';
      break;
    case '6':
      commit_id[i] = '1';
      return;
    case '1':
      commit_id[i] = 'c';
      return;
    default:
      fprintf(stderr, "ERROR: commit id only contain charactors in ['6', '1', 'c'], origin commit id \"%s\"\n", commit_id);
      return;
    }
    i++;
  }
}

int beargit_commit_hw1(const char *msg)
{
  if (!is_commit_msg_ok(msg))
  {
    fprintf(stderr, "ERROR: Message must contain \"%s\"\n", go_bears);
    return 1;
  }
  char commit_id[COMMIT_ID_SIZE];
  char commit_dir[FILENAME_SIZE];
  char file_name[FILENAME_SIZE];
  char line[FILENAME_SIZE];

  read_string_from_file(".beargit/.prev", commit_id, COMMIT_ID_SIZE);
  next_commit_id(commit_id);

  sprintf(commit_dir, ".beargit/%s", commit_id);
  fs_mkdir(commit_dir);

  sprintf(file_name, "%s/.index", commit_dir);
  fs_cp(".beargit/.index", file_name);
  sprintf(file_name, "%s/.prev", commit_dir);
  fs_cp(".beargit/.prev", file_name);

  FILE *findex = fopen(".beargit/.index", "r");
  while (fgets(line, sizeof(line), findex))
  {
    strtok(line, "\n");
    sprintf(file_name, "%s/%s", commit_dir, line);
    fs_cp(line, file_name);
  }

  sprintf(file_name, "%s/.msg", commit_dir);
  write_string_to_file(file_name, msg);

  write_string_to_file(".beargit/.prev", commit_id);

  fclose(findex);
  return 0;
}

/* beargit status */

int beargit_status()
{
  FILE *findex = fopen(".beargit/.index", "r");
  char line[FILENAME_SIZE];
  int total_files = 0;

  fprintf(stdout, "Tracked files:\n\n");
  while (fgets(line, sizeof(line), findex))
  {
    strtok(line, "\n");
    fprintf(stdout, "  %s\n", line);
    total_files++;
  }
  fprintf(stdout, "\n%d files total\n", total_files);
  fclose(findex);

  return 0;
}

/* beargit log */

int beargit_log()
{
  char commit_id[COMMIT_ID_SIZE];
  char file_name[FILENAME_SIZE];
  char msg[MSG_SIZE];
  read_string_from_file(".beargit/.prev", commit_id, COMMIT_ID_SIZE);

  if (!strcmp(commit_id, "0000000000000000000000000000000000000000"))
  {
    fprintf(stderr, "ERROR: There are no commits!\n");
    return 1;
  }
  fprintf(stdout, "\n");

  while (strcmp(commit_id, "0000000000000000000000000000000000000000"))
  {
    sprintf(file_name, ".beargit/%s/.msg", commit_id);
    read_string_from_file(file_name, msg, MSG_SIZE);

    fprintf(stdout, "commit %s\n", commit_id);
    fprintf(stdout, "    %s\n\n", msg);

    sprintf(file_name, ".beargit/%s/.prev", commit_id);
    read_string_from_file(file_name, commit_id, COMMIT_ID_SIZE);
  }

  return 0;
}

// This adds a check to beargit_commit that ensures we are on the HEAD of a branch.
int beargit_commit(const char *msg)
{
  char current_branch[BRANCHNAME_SIZE];
  read_string_from_file(".beargit/.current_branch", current_branch, BRANCHNAME_SIZE);

  if (strlen(current_branch) == 0)
  {
    fprintf(stderr, "ERROR: Need to be on HEAD of a branch to commit\n");
    return 1;
  }

  return beargit_commit_hw1(msg);
}

const char *digits = "61c";

void next_commit_id(char *commit_id)
{
  char current_branch[BRANCHNAME_SIZE];
  read_string_from_file(".beargit/.current_branch", current_branch, BRANCHNAME_SIZE);

  // The first COMMIT_ID_BRANCH_BYTES=10 characters of the commit ID will
  // be used to encode the current branch number. This is necessary to avoid
  // duplicate IDs in different branches, as they can have the same pre-
  // decessor (so next_commit_id has to depend on something else).
  int n = get_branch_number(current_branch);
  for (int i = 0; i < COMMIT_ID_BRANCH_BYTES; i++)
  {
    // This translates the branch number into base 3 and substitutes 0 for
    // 6, 1 for 1 and c for 2.
    commit_id[i] = digits[n % 3];
    n /= 3;
  }

  // Use next_commit_id to fill in the rest of the commit ID.
  next_commit_id_hw1(commit_id + COMMIT_ID_BRANCH_BYTES);
}

// This helper function returns the branch number for a specific branch, or
// returns -1 if the branch does not exist.
int get_branch_number(const char *branch_name)
{
  FILE *fbranches = fopen(".beargit/.branches", "r");

  int branch_index = -1;
  int counter = 0;
  char line[FILENAME_SIZE];
  while (fgets(line, sizeof(line), fbranches))
  {
    strtok(line, "\n");
    if (strcmp(line, branch_name) == 0)
    {
      branch_index = counter;
    }
    counter++;
  }

  fclose(fbranches);

  return branch_index;
}

/* beargit branch */

int beargit_branch()
{
  FILE *file = fopen(".beargit/.branches", "r");
  char cur_branch[BRANCHNAME_SIZE];
  char line[BRANCHNAME_SIZE];
  read_string_from_file(".beargit/.current_branch", cur_branch, BRANCHNAME_SIZE);
  strtok(cur_branch, "\n");
  while (fgets(line, sizeof(line), file))
  {
    strtok(line, "\n");
    if ((strcmp(line, cur_branch) == 0))
      fprintf(stdout, "*  %s\n", line);
    else
      fprintf(stdout, "   %s\n", line);
  }
  fclose(file);
  return 0;
}

/* beargit checkout */

int checkout_commit(const char *commit_id)
{
  char file[FILENAME_SIZE];
  FILE *findex = fopen(".beargit/.index", "r");
  // Delete all files in pwd
  while (fgets(file, sizeof(file), findex))
  {
    strtok(file, "\n");
    fs_rm(file);
  }
  fclose(findex);

  // Copy .index file
  sprintf(file, ".beargit/%s/.index", commit_id);
  fs_cp(file, ".beargit/.index");

  if ((strcmp(commit_id, "0000000000000000000000000000000000000000")) == 0)
    write_string_to_file(".beargit/.index", "");
  else
  {
    // Copy all files mentioned in .index
    findex = fopen(".beargit/.index", "r");
    while (fgets(file, sizeof(file), findex))
    {
      strtok(file, "\n");
      char old[FILENAME_SIZE];
      sprintf(old, ".beargit/%s/%s", commit_id, file);
      fs_cp(old, file);
    }
    fclose(findex);
  }
  write_string_to_file(".beargit/.prev", commit_id);

  return 0;
}

int is_it_a_commit_id(const char *commit_id)
{
  if (strlen(commit_id) != 40)
    return 0;

  int i = 0;
  while (commit_id[i] != '\0')
  {
    char c = commit_id[i];
    if (!(c == '6' || c == '1' || c == 'c'))
      return 0;
    i += 1;
  }
  return 1;
}

int beargit_checkout(const char *arg, int new_branch)
{
  // Get the current branch
  char current_branch[BRANCHNAME_SIZE];
  read_string_from_file(".beargit/.current_branch", current_branch, BRANCHNAME_SIZE);

  // If not detached, update the current branch by storing the current HEAD into that branch's file...
  // Even if we cancel later, this is still ok.
  if (strlen(current_branch))
  {
    char current_branch_file[BRANCHNAME_SIZE + 50];
    sprintf(current_branch_file, ".beargit/.branch_%s", current_branch);
    fs_cp(".beargit/.prev", current_branch_file);
  }

  // Check whether the argument is a commit ID. If yes, we just stay in detached mode
  // without actually having to change into any other branch.
  if (is_it_a_commit_id(arg))
  {
    char commit_dir[FILENAME_SIZE] = ".beargit/";
    strcat(commit_dir, arg);
    if (!fs_check_dir_exists(commit_dir))
    {
      fprintf(stderr, "ERROR: Commit %s does not exist\n", arg);
      return 1;
    }

    // Set the current branch to none (i.e., detached).
    write_string_to_file(".beargit/.current_branch", "");

    return checkout_commit(arg);
  }

  // Just a better name, since we now know the argument is a branch name.
  const char *branch_name = arg;

  // Read branches file (giving us the HEAD commit id for that branch).
  int branch_exists = (get_branch_number(branch_name) >= 0);

  // Check for errors.
  if (new_branch && branch_exists)
  {
    fprintf(stderr, "ERROR: A branch named %s already exists\n", branch_name);
    return 1;
  }
  else if (!new_branch && !branch_exists)
  {
    fprintf(stderr, "ERROR: No branch %s exists\n", branch_name);
    return 1;
  }

  // File for the branch we are changing into.
  char branch_file[BRANCHNAME_SIZE + 50];
  sprintf(branch_file, ".beargit/.branch_%s", branch_name);

  // Update the branch file if new branch is created (now it can't go wrong anymore)
  if (new_branch)
  {
    FILE *fbranches = fopen(".beargit/.branches", "a");
    fprintf(fbranches, "%s\n", branch_name);
    fclose(fbranches);
    fs_cp(".beargit/.prev", branch_file);
  }

  write_string_to_file(".beargit/.current_branch", branch_name);

  // Read the head commit ID of this branch.
  char branch_head_commit_id[COMMIT_ID_SIZE];
  read_string_from_file(branch_file, branch_head_commit_id, COMMIT_ID_SIZE);

  // Check out the actual commit.
  return checkout_commit(branch_head_commit_id);
}
