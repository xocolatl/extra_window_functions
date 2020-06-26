#include "postgres.h"
#include "fmgr.h"

#include "windowapi.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(window_lag_ignore_nulls);
PG_FUNCTION_INFO_V1(window_lag_ignore_nulls_with_offset);
PG_FUNCTION_INFO_V1(window_lag_ignore_nulls_with_offset_with_default);

PG_FUNCTION_INFO_V1(window_lead_ignore_nulls);
PG_FUNCTION_INFO_V1(window_lead_ignore_nulls_with_offset);
PG_FUNCTION_INFO_V1(window_lead_ignore_nulls_with_offset_with_default);

PG_FUNCTION_INFO_V1(window_first_value_ignore_nulls);
PG_FUNCTION_INFO_V1(window_first_value_ignore_nulls_with_default);

PG_FUNCTION_INFO_V1(window_last_value_ignore_nulls);
PG_FUNCTION_INFO_V1(window_last_value_ignore_nulls_with_default);

PG_FUNCTION_INFO_V1(window_nth_value_from_last);
PG_FUNCTION_INFO_V1(window_nth_value_from_last_ignore_nulls);
PG_FUNCTION_INFO_V1(window_nth_value_from_last_ignore_nulls_with_default);
PG_FUNCTION_INFO_V1(window_nth_value_from_last_with_default);
PG_FUNCTION_INFO_V1(window_nth_value_ignore_nulls);
PG_FUNCTION_INFO_V1(window_nth_value_ignore_nulls_with_default);
PG_FUNCTION_INFO_V1(window_nth_value_with_default);

PG_FUNCTION_INFO_V1(window_flip_flop_1);
PG_FUNCTION_INFO_V1(window_flip_flop_2);

typedef struct flip_flop_context
{
	bool	flip_flop;
} flip_flop_context;

/* LEAD / LAG COMMON */

static Datum
lead_lag_common(
		FunctionCallInfo fcinfo,
		bool forward, bool with_offset, bool with_default)
{
	WindowObject winobj = PG_WINDOW_OBJECT();
	int32		offset;
	/*bool		const_offset;*/
	Datum		result;
	bool		isnull;
	bool		isout;
	int			step = forward ? 1 : -1;
	int			runner = step;

	if (with_offset)
	{
		offset = DatumGetInt32(WinGetFuncArgCurrent(winobj, 1, &isnull));
		if (isnull)
			PG_RETURN_NULL();
	}
	else
		offset = 1;

	if (!forward)
		offset = -offset;

	if (offset == 0)
		runner = 0;

	for (;;)
	{
		result = WinGetFuncArgInPartition(winobj, 0,
										  runner,
										  WINDOW_SEEK_CURRENT,
										  false,
										  &isnull, &isout);

		if (isout)
			break;

		if (isnull)
			offset += step;

		if (runner == offset)
			break;

		runner += step;
	}

	if (isout)
	{
		/*
		 * target row is out of the partition; supply default value if
		 * provided.  otherwise it'll stay NULL
		 */
		if (with_default)
			result = WinGetFuncArgCurrent(winobj, 2, &isnull);
	}

	if (isnull)
		PG_RETURN_NULL();

	PG_RETURN_DATUM(result);
}

Datum
window_lag_ignore_nulls(PG_FUNCTION_ARGS)
{
	return lead_lag_common(fcinfo, false, false, false);
}

Datum
window_lag_ignore_nulls_with_offset(PG_FUNCTION_ARGS)
{
	return lead_lag_common(fcinfo, false, true, false);
}

Datum
window_lag_ignore_nulls_with_offset_with_default(PG_FUNCTION_ARGS)
{
	return lead_lag_common(fcinfo, false, true, true);
}

Datum
window_lead_ignore_nulls(PG_FUNCTION_ARGS)
{
	return lead_lag_common(fcinfo, true, false, false);
}

Datum
window_lead_ignore_nulls_with_offset(PG_FUNCTION_ARGS)
{
	return lead_lag_common(fcinfo, true, true, false);
}

Datum
window_lead_ignore_nulls_with_offset_with_default(PG_FUNCTION_ARGS)
{
	return lead_lag_common(fcinfo, true, true, true);
}

/* FIRST_VALUE / LAST_VALUE COMMON */

static Datum
first_last_value_common(
		FunctionCallInfo fcinfo,
		bool from_last, bool with_default)
{
	WindowObject winobj = PG_WINDOW_OBJECT();
	Datum		result;
	bool		isnull;
	bool		isout;
	int			runner = 0;
	int			step = from_last ? -1 : 1;

	for (;;)
	{
		result = WinGetFuncArgInFrame(
					winobj, 0, runner,
					from_last ? WINDOW_SEEK_TAIL : WINDOW_SEEK_HEAD,
					false, &isnull, &isout);

		if (isout || !isnull)
			break;

		runner += step;
	}

	if (isout && with_default)
		result = WinGetFuncArgCurrent(winobj, 1, &isnull);

	if (isnull)
		PG_RETURN_NULL();

	PG_RETURN_DATUM(result);
}

/* FIRST_VALUE */

Datum
window_first_value_ignore_nulls(PG_FUNCTION_ARGS)
{
	return first_last_value_common(fcinfo, false, false);
}

Datum
window_first_value_ignore_nulls_with_default(PG_FUNCTION_ARGS)
{
	return first_last_value_common(fcinfo, false, true);
}

/* LAST_VALUE */

Datum
window_last_value_ignore_nulls(PG_FUNCTION_ARGS)
{
	return first_last_value_common(fcinfo, true, false);
}

Datum
window_last_value_ignore_nulls_with_default(PG_FUNCTION_ARGS)
{
	return first_last_value_common(fcinfo, true, true);
}

/* NTH_VALUE COMMON */

static Datum
nth_value_common(
		FunctionCallInfo fcinfo, const char *fname,
		bool from_last, bool ignore_nulls, bool with_default)
{
	WindowObject winobj = PG_WINDOW_OBJECT();
	Datum		result;
	bool		isnull;
	bool		isout;
	int32		nth;
	int			seektype;

	nth = DatumGetInt32(WinGetFuncArgCurrent(winobj, 1, &isnull)) - 1;
	if (isnull)
		PG_RETURN_NULL();

	if (nth < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_ARGUMENT_FOR_NTH_VALUE),
				 errmsg("argument of %s must be greater than zero", fname)));

	if (from_last)
	{
		seektype = WINDOW_SEEK_TAIL;
		nth = -nth;
	}
	else
		seektype = WINDOW_SEEK_HEAD;

	if (ignore_nulls)
	{
		int		runner = 0;
		int		step = from_last ? -1 : 1;

		for (;;)
		{
			result = WinGetFuncArgInFrame(winobj, 0,
										  runner, seektype, false,
										  &isnull, &isout);

			if (isout)
				break;

			if (isnull)
				nth += step;

			if (runner == nth)
				break;

			runner += step;
		}
	}
	else
	{
		bool	const_offset = get_fn_expr_arg_stable(fcinfo->flinfo, 1);
		result = WinGetFuncArgInFrame(winobj, 0,
									  nth, seektype, const_offset,
									  &isnull, &isout);
	}

	if (isout && with_default)
		result = WinGetFuncArgCurrent(winobj, 2, &isnull);

	if (isnull)
		PG_RETURN_NULL();

	PG_RETURN_DATUM(result);
}

/* NTH_VALUE */

Datum
window_nth_value_from_last(PG_FUNCTION_ARGS)
{
	return nth_value_common(fcinfo, "nth_value_from_last", true, false, false);
}

Datum
window_nth_value_from_last_ignore_nulls(PG_FUNCTION_ARGS)
{
	return nth_value_common(fcinfo, "nth_value_from_last_ignore_nulls", true, true, false);
}

Datum
window_nth_value_from_last_ignore_nulls_with_default(PG_FUNCTION_ARGS)
{
	return nth_value_common(fcinfo, "nth_value_from_last_ignore_nulls", true, true, true);
}

Datum
window_nth_value_from_last_with_default(PG_FUNCTION_ARGS)
{
	return nth_value_common(fcinfo, "nth_value_from_last", true, false, true);
}

Datum
window_nth_value_ignore_nulls(PG_FUNCTION_ARGS)
{
	return nth_value_common(fcinfo, "nth_value_ignore_nulls", false, true, false);
}

Datum
window_nth_value_ignore_nulls_with_default(PG_FUNCTION_ARGS)
{
	return nth_value_common(fcinfo, "nth_value_ignore_nulls", false, true, true);
}

Datum
window_nth_value_with_default(PG_FUNCTION_ARGS)
{
	return nth_value_common(fcinfo, "nth_value", false, false, true);
}

/* FLIP_FLOP */

static Datum
flip_flop(FunctionCallInfo fcinfo, int flip_argno, int flop_argno)
{
	WindowObject		winobj = PG_WINDOW_OBJECT();
	flip_flop_context  *context;
	bool				isnull;

	context = (flip_flop_context *)
		WinGetPartitionLocalMemory(winobj, sizeof(flip_flop_context));

	if (!context->flip_flop)
	{
		bool	flip;

		flip = WinGetFuncArgCurrent(winobj, flip_argno, &isnull);
		if (!isnull && flip)
		{
			context->flip_flop = true;
			PG_RETURN_BOOL(true);
		}

		PG_RETURN_BOOL(false);
	}
	else
	{
		bool	flop;

		flop = WinGetFuncArgCurrent(winobj, flop_argno, &isnull);
		if (!isnull && flop)
			context->flip_flop = false;

		PG_RETURN_BOOL(true);
	}
}

Datum
window_flip_flop_1(PG_FUNCTION_ARGS)
{
	return flip_flop(fcinfo, 0, 0);
}

Datum
window_flip_flop_2(PG_FUNCTION_ARGS)
{
	return flip_flop(fcinfo, 0, 1);
}
