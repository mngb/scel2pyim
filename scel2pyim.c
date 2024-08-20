/* Copyright (C) 2015-2020 E-Neo <e-neo@qq.com>

   This file is part of scel2pyim.

   scel2pyim is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   scel2pyim is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with scel2pyim.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <errno.h>
#include <strings.h>

#define MAX_LINE_LENGTH 4096
// 最长的输入条目，过长的将被过滤掉
#define MAX_ITEM_LINE_LENGTH 128

int is_directory(const char *path) {
    struct stat statbuf;
    
    if (stat(path, &statbuf) != 0) {
        // 如果获取状态失败，返回 0 表示不是目录
        return 0;
    }
    
    return S_ISDIR(statbuf.st_mode);
}

static int
unicode2utf8char (unsigned short in, char *out)
{
  if (in <= 0x007f)
    {
      out[0] = in;
      return 1;
    }
  else if (in >= 0x0080 && in <= 0x07ff)
    {
      out[0] = 0xc0 | (in >> 6);
      out[1] = 0x80 | (in & 0x3f);
      return 2;
    }
  else if (in >= 0x0800)
    {
      out[0] = 0xe0 | (in >> 12);
      out[1] = 0x80 | ((in >> 6) & 0x3f);
      out[2] = 0x80 | (in & 0x3f);
      return 3;
    }
  else
    return -1;
}

static int
unicode2utf8str (unsigned short *in, int insize, char *out)
{
  char str[3 + 1];
  int i;
  *out = 0;
  for (i = 0; i < insize; i++)
    {
      if (in[i] > 0 && in[i] < 20)
        continue;
      str[unicode2utf8char (in[i], str)] = 0;
      strcat (out, str);
      if (in[i] == 0)
        break;
    }
  return 0;
}

static void
mkpydict (FILE *in, char *pydict[]) /* Create a pinyin dictionary. */
{
  int i;
  unsigned short addr, size, code[6];
  char *p;
  fseek (in, 0x1544, SEEK_SET);
  for (i = 0; i < 413; i++)
    {
      p = (char *)malloc (7);
      fread (&addr, 2, 1, in);
      fread (&size, 2, 1, in);
      fread (code, 2, size / 2, in);
      unicode2utf8str (code, size / 2, p);
      pydict[addr] = p;
    }
}

static int
mkpyimpylist (char *pydict[], unsigned short pysize,
              unsigned short pylistcode[], char pylist[])
{
  unsigned short i, j = pysize - 1;
  for (i = 0; i < pysize; i++)
    {
      if (pylistcode[i] >= 413) {
        return 0;
      }
      strcat (pylist, pydict[pylistcode[i]]);
      if (j)
        {
          strcat (pylist, "-");
          j--;
        }
    }
  return 1;
}

static void
writepyim (FILE *in, FILE *out, char *pydict[])
{
  unsigned short same, pysize, *pylistcode;
  unsigned short *wdsize, *wdlistcode, extrsize, *extrlist, i, j;
  char *pylist, *wdlist;
  long end;
  fseek (in, 0, SEEK_END);
  end = ftell (in);
  fseek (in, 0x2628, SEEK_SET);
  while (ftell (in) != end)
    {
      fread (&same, 2, 1, in);
      fread (&pysize, 2, 1, in);
      pylistcode = (unsigned short *)malloc (pysize);
      fread (pylistcode, 2, pysize / 2, in);
      pylist = (char *)malloc (pysize * 7);
      pylist[0] = 0;
      int ret = mkpyimpylist (pydict, pysize / 2, pylistcode, pylist);
     
      fprintf (out, "%s ", pylist);
      free (pylistcode);
      free (pylist);
      if (!ret) {
        printf("invalid file!\n");
        break;
      }
      wdsize = (unsigned short *)malloc (same * 2);
      for (i = 0, j = same - 1; i < same; i++, j--)
        {
          fread (&wdsize[i], 2, 1, in);
          wdlistcode = (unsigned short *)malloc (wdsize[i]);
          fread (wdlistcode, 2, wdsize[i] / 2, in);
          wdlist = (char *)malloc (3 * wdsize[i] + 1);
          unicode2utf8str (wdlistcode, wdsize[i] / 2, wdlist);
          free (wdlistcode);
          fprintf (out, "%s", wdlist);
          if (j)
            fprintf (out, " ");
          free (wdlist);
          fread (&extrsize, 2, 1, in);
          extrlist = (unsigned short *)malloc (extrsize);
          fread (extrlist, 2, extrsize / 2, in);
          free (extrlist);
        }
      free (wdsize);
      fprintf (out, "\n");
    }
}

static int
readandwrite (FILE *in, FILE *out)
{
  char buffer[5], *pydict[413];
  fseek (in, 0x1540, SEEK_SET);
  fread (buffer, 1, 4, in);
  if (memcmp (buffer, "\x9D\x01\x00\x00", 4) != 0)
    return -1;
  mkpydict (in, pydict);
  writepyim (in, out, pydict);
  for (size_t i = 0; i < 413; i++)
    free (pydict[i]);
  return 0;
}

struct convert_params {
  FILE* pyim;
  char scel_path[1024];
};

void
convert_scel_to_pyim(struct convert_params* args)
{
  FILE *in;
  // printf("%s\n", args->scel_path);
  if ((in = fopen (args->scel_path, "rb")) == NULL)
  {
    fprintf (stderr, "Could not find \"%s\"\n", args->scel_path);
    return;
  }
  char buffer[12];
  fread (buffer, 1, 12, in);
  if (memcmp (buffer, "\x40\x15\x00\x00\x44\x43\x53\x01\x01\x00\x00\x00", 12))
  {
    fclose (in);
    fprintf (stderr, "\"%s\" is not a valid scel file.\n", args->scel_path);
    return;
  }
  if (readandwrite (in, args->pyim) < 0)
  {
    fprintf (stderr, "\"%s\" is not a valid scel file while writing to pyim.\n", args->scel_path);
    return;
  }
  fclose (in);
  return;
}

void
work_directory(char* dirname, struct convert_params* param, void (* func)(struct convert_params*))
{
  DIR *dir;
  struct dirent *entry;

  dir = opendir(dirname);
  if (dir == NULL) {
    fprintf (stderr, "Dir %s open failure!\n", dirname);
    return;
  }

  const char* src_suffix = ".scel";
  const int suffix_len = strlen(src_suffix);
  while ((entry = readdir(dir)) != NULL) {
    int item_len = strlen(entry->d_name);
    if (suffix_len < item_len && strcasecmp(src_suffix, &(entry->d_name[item_len-suffix_len]))==0) {
      sprintf(param->scel_path, "%s/%s", dirname, entry->d_name);
      func(param);
    } else if(strcmp(entry->d_name, ".")!=0 && strcmp(entry->d_name, "..")!=0){
      char subdir[512];
      sprintf(subdir, "%s/%s", dirname, entry->d_name);
      if (is_directory(subdir)) {
        work_directory(subdir, param, func);
      }
    }
  }
  closedir(dir);
  return;
}

struct word_split_buf {
  char words[32][MAX_ITEM_LINE_LENGTH];
  int split_count;
};

static void transfer_line_to_split_buf(struct word_split_buf* buf, const char* s) {
  const char* final_pos = strchr(s, ' ');
  const char* first_pos = s;
  const char* start_pos = s; // 指向最开始的 word 字符
  const char* end_pos = s; // 指向最后的 word 字符，非 -
  
  buf->split_count = 0;
  while( s != final_pos ) {
    if (*s == '-') {
      end_pos = s - 1;
      memcpy(buf->words[buf->split_count], start_pos, end_pos - start_pos + 1);
      buf->words[buf->split_count][end_pos - start_pos + 1] = 0;
      start_pos = s + 1;
      buf->split_count += 1;
    }
    s++;
  }
  end_pos = s - 1;
  memcpy(buf->words[buf->split_count], start_pos, end_pos - start_pos + 1);
  buf->words[buf->split_count][end_pos - start_pos + 1] = 0;
  buf->split_count += 1;
}
    
int compare(const void *a, const void *b) {
  struct word_split_buf s1;
  struct word_split_buf s2;
  int cmp_result = 0;
  transfer_line_to_split_buf(&s1, *(const char **)a);
  transfer_line_to_split_buf(&s2, *(const char **)b);

  // cmp  word_split_buf
  int max_count = s1.split_count > s2.split_count ? s1.split_count : s2.split_count;
  for (int i=0; i<max_count; i++) {
    if (i >= s1.split_count) {
      cmp_result = -1;
      break;
    }
    if (i >= s2.split_count) {
      cmp_result = 1;
      break;
    }
    int tmp_result = strcmp(s1.words[i], s2.words[i]);
    if (tmp_result != 0) {
      cmp_result = tmp_result;
      break;
    }
  }

  // printf("%d => %s : %s\n", cmp_result, *(const char **)a, *(const char **)b);
  return cmp_result;
}

void sort_pyim_file(char* filename)
{
    FILE *file = fopen(filename, "rb+");
    if (!file) {
        return;
    }
    fseek(file, 0, SEEK_END);
    int file_size = (int)(ftell(file));
    fseek(file, 0, SEEK_SET);
    
    char* file_content = (char*)malloc(file_size);
    char* cur_file_point = file_content;
    char* first_file_point = NULL;
    int line_count = 0;
    
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, MAX_LINE_LENGTH, file)) {
      // 忽略第一行，为 ";; -*- coding: utf-8-unix; -*-"
      // 需要去掉换行符，避免 file_content 溢出
      int line_len = strlen(buffer);
      if (line_len > MAX_ITEM_LINE_LENGTH - 1) {
        printf("ignored too long line: %s\n", buffer);
        continue;
      }
      memcpy(cur_file_point, buffer, line_len);
      cur_file_point[line_len-1] = 0;
      cur_file_point += line_len;
      line_count++;
      if (line_count==1) {
        first_file_point = cur_file_point;
      }
    }

    char** lines = (char**) malloc(sizeof(char*) * (line_count - 1));
    lines[0] = first_file_point;
    for(int i=1; i< line_count - 1; i++) {
      // printf("%d: %s\n", i-1, lines[i-1]);
      lines[i] = lines[i-1] + strlen(lines[i-1]) + 1;
    }

    // 使用 qsort 对行进行排序
    qsort(lines, line_count-1, sizeof(char *), compare);

    // 打开输出文件
    file = freopen(NULL, "w+", file);
    // 写入首行
    fprintf(file, "%s", file_content);
    
    // 将排序后的行写入输出文件
    const char** targets = (const char**) malloc(sizeof(char*) * (line_count - 1));
    int target_num = 0;
    char cur_input_prefix[MAX_ITEM_LINE_LENGTH] = {0};
    
    for (int i = 0; i < line_count-1; i++) {
      const char* split_pos = strchr(lines[i], ' ');
      if (split_pos==NULL) {
        continue;
      }
      const char* cur_target_pos = split_pos + 1; // 假定了 prefix 和 target 之间只能有一个空格
      int prefix_len = split_pos - lines[i];
      if (memcmp(cur_input_prefix, lines[i], prefix_len)) {
        // write all targets
        for(int j=0; j<target_num; j++) {
          if (!strchr(targets[j], ' ')) { // target 中有空格的为不合法的 target
            fprintf(file, "%s %s\n", cur_input_prefix, targets[j]);
          }
        }
        target_num = 0;
        memcpy(cur_input_prefix, lines[i], prefix_len);
        cur_input_prefix[prefix_len] = '\0';
        targets[target_num] = cur_target_pos;
        target_num++;
      } else {
        int j;
        for(j=0; j<target_num; j++) {
          if (!strcmp(targets[j], cur_target_pos)) {
            break;
          }
        }
        if (j==target_num) {
          // new target
          targets[target_num] = cur_target_pos;
          target_num++;
        }
      }
    }
    free(targets);
    free(lines);
    free(file_content);
    fclose(file);
}

int
main (int argc, char *argv[])
{
  FILE *in, *out;
  if (argc != 3)
    {
      fprintf (stderr, "Usage : %s /path/to/ /path/to/NAME.pyim\n",
               argv[0]);
      return EXIT_FAILURE;
    }
  if (!is_directory(argv[1])) {
    fprintf (stderr, "%s is not a directory!\n",
               argv[1]);
      return EXIT_FAILURE;
  }

  struct convert_params params;
  
  if ((out = fopen (argv[2], "wb")) == NULL)
  {
    fprintf (stderr, "Could not open \"%s\"\n", argv[2]);
    return EXIT_FAILURE;
  }
  fprintf (out, ";; -*- coding: utf-8-unix; -*-\n");
  params.pyim = out;
  work_directory(argv[1], &params, convert_scel_to_pyim);
  fclose (out);

  // 对 .pyim 重新排序
  sort_pyim_file(argv[2]);
  return 0;
}
