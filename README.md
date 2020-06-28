# Extra Window Functions for PostgreSQL

[![License](https://img.shields.io/badge/license-PostgreSQL-blue)](https://www.postgresql.org/about/licence/)
[![Code of Conduct](https://img.shields.io/badge/code%20of%20conduct-PostgreSQL-blueviolet)](https://www.postgresql.org/about/policies/coc/)

[![Travis Build Status](https://api.travis-ci.com/xocolatl/extra_window_functions.svg?branch=master)](https://travis-ci.com/xocolatl/extra_window_functions)

*compatible 9.6–13*

This extension provides additional window functions to PostgreSQL.  Some of
them provide SQL Standard functionality but without the SQL Standard grammar,
others extend on the SQL Standard, and still others are novel and hopefully
useful to someone.

## Simulating Standard SQL

The window functions `LEAD()`, `LAG()`, `FIRST_VALUE()`, `LAST_VALUE()`, and
`NTH_VALUE()` can skip over null values.  PostgreSQL does not implement the
syntax required for that feature but this extension provides additional
functions that give you the same behavior.

In addition to this, `NTH_VALUE()` can count from the start or the end of the
window frame.

Despite these functions having long names, there isn't really any difference in
length compared to the excessively verbose SQL Standard syntax.

```
-- Standard SQL:
NTH_VALUE(x, 3) FROM LAST IGNORE NULLS OVER w

-- This extension:
nth_value_from_last_ignore_nulls(x, 3) OVER w
```

## Extending Standard SQL

The functions `LEAD()` and `LAG()` accept a default value for when the
requested row falls outside of the partition.  However, the functions
`FIRST_VALUE()`, `LAST_VALUE()`, and `NTH_VALUE()` do not have default values
for when the requested row is not in the frame.

## Non-Standard Functions

This extension introduces a new partition-level window function `flip_flop()`
and implements the
"[flip floperator](https://en.wikipedia.org/wiki/Flip-flop_(programming))".

In the first variant, the function returns false until the expression given as
an argument returns true.  It then keeps returning true until the expression is
matched again.  The second variant takes two expressions: the first to flip,
the second to flop.
