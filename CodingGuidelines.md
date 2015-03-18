# General #
  * Use const variables or enums instead of macros
  * Use CamelCase for types and function names
  * Use pascalCase for variable names
  * One level of indentation is 4 spaces (no tabs)

# Classes #
  * Names of interfaces are prefixed with I.  Class names get no prefix
  * Refer to member variables and functions with the this pointer
  * Data member names don't contain a prefix
  * Use an access modifier for each declaration in a class

# Accessors #
  * Use Property(), SetProperty(..) instead of GetProperty(), SetProprety(...), e.g:

```
   class Font
   {
   ...
   public:    int Size();
   public:    void SetSize(int newSize);
   ...
   };
```

# Subversion #
  * The main project in the trunk should always build and work.
  * There may be code that is not working, but that should be unused or excluded by the build.

# Issue tracking #
  * Always set Status as "Started" on issues you are currently working on.
  * Devs can have multiple issues assigned to them in the "Accepted" state as some kind of personal todo list.
  * Bigger issues should probably be branched. Discuss this on the IRC channel with the other developers.
  * New feature ideas should be added as "New" and not assigned to anyone.  We should discuss acceptance and priorities of new features with the devs (on IRC?).