/*
 * Functions for managing the connection to the backend.
 */

#include "main.h"

#include "libpq-fe.h"

static PGconn *conn = NULL;

bool
db_is_connected(void)
{
	return conn != NULL;
}

void
db_connect(void)
{
	/*
	 * FIXME: ATM, you have to use env variables to specify where and how to
	 * connect.
	 */
	conn = PQconnectdb("");

	/* Check to see that the backend connection was successfully made */
	if (PQstatus(conn) != CONNECTION_OK)
	{
		reporterror("Connection to database failed: %s",
				PQerrorMessage(conn));
		PQfinish(conn);
		conn = NULL;
	}
}

relation_info *
db_fetch_relations(int *nrels)
{
	PGresult   *res;
	relation_info *result;
	int			i;

	res = PQexec(conn, "SELECT relname, relpages, relkind FROM pg_catalog.pg_class ORDER BY relname");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		reporterror("could not get relation list: %s", PQerrorMessage(conn));
		PQclear(res);
		return NULL;
	}
	
	/* first, print out the attribute names */
	if (PQnfields(res) != 3)
	{
		reporterror("unexpected result set");
		PQclear(res);
		return NULL;
	}

	*nrels = PQntuples(res);
	result = pg_malloc(sizeof(relation_info) * (*nrels));

	for (i = 0; i < *nrels; i++)
	{
		relation_info *rel = &result[i];

		strncpy(rel->relname, PQgetvalue(res, i, 0), sizeof(rel->relname));
		rel->nblocks = strtoul(PQgetvalue(res, i, 1), NULL, 10);
		rel->relkind = *PQgetvalue(res, i, 2);
	}
	PQclear(res);
	return result;
}

char *
db_fetch_block(char *relname, char *forkname, BlockNumber blkno)
{
	const char *params[3];
	char		blknobuf[10];
	PGresult   *res;
	char	   *result;

	snprintf(blknobuf, sizeof(blknobuf), "%d", blkno);

	params[0] = relname;
	params[1] = forkname;
	params[2] = blknobuf;
	
	/* FIXME: schema-qualify this? */
	res = PQexecParams(conn, "SELECT get_raw_page($1, $2, $3)", 3,
					   NULL, params, NULL, NULL,
					   1); /* binary result */
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		reporterror("get_raw_page call failed: %s", PQerrorMessage(conn));
		PQclear(res);
		return NULL;
	}

	/* first, print out the attribute names */
	if (PQnfields(res) != 1 || PQntuples(res) != 1)
	{
		reporterror("unexpected result set from get_raw_page()");
		PQclear(res);
		return NULL;
	}

	if (PQgetlength(res, 0, 0) != BLCKSZ)
	{
		reporterror("unexpected result length from get_raw_page: %d, expected %d",
					PQgetlength(res, 0, 0), BLCKSZ);
		PQclear(res);
		return NULL;
	}

	result = pg_malloc(BLCKSZ);
	memcpy(result, PQgetvalue(res, 0, 0), BLCKSZ);

	PQclear(res);
	return result;
}
