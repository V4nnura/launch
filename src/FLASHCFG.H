/* Configuration-only accessibility preference. Missing key defaults to true.
   Explicit values other than true fail closed (no flashing). */
static int flashing_value(const char *value)
{
  char word[6];int n=0;
  while(*value&&isspace((unsigned char)*value))value++;
  while(*value&&!isspace((unsigned char)*value)&&n<5)
    { word[n++]=(char)tolower((unsigned char)*value);value++; }
  word[n]=0;
  while(*value&&isspace((unsigned char)*value))value++;
  return !*value&&!strcmp(word,"true");
}
