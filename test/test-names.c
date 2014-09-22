#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <stdbool.h>

#include "kdbus-util.h"
#include "kdbus-enum.h"
#include "kdbus-test.h"

static int conn_is_name_owner(const struct kdbus_conn *conn,
			      uint64_t flags, const char *n)
{
	struct kdbus_cmd_name_list cmd_list;
	struct kdbus_name_list *list;
	struct kdbus_cmd_name *name;
	bool found = false;
	int ret;

	cmd_list.flags = flags;

	ret = ioctl(conn->fd, KDBUS_CMD_NAME_LIST, &cmd_list);
	ASSERT_RETURN(ret == 0);

	list = (struct kdbus_name_list *)(conn->buf + cmd_list.offset);
	KDBUS_ITEM_FOREACH(name, list, names) {
		if (name->size == sizeof(struct kdbus_cmd_name))
			continue;

		if (name->owner_id == conn->id &&
		    strcmp(n, name->name) == 0) {
			found = true;
			break;
		}
	}

	ret = kdbus_free(conn, cmd_list.offset);
	ASSERT_RETURN(ret == 0);

	return found ? 0 : -1;
}

int kdbus_test_name_basic(struct kdbus_test_env *env)
{
	char *name;
	int ret;

	name = "foo.bla.blaz";

	/* check that we can acquire a name */
	ret = kdbus_name_acquire(env->conn, name, NULL);
	ASSERT_RETURN(ret == 0);

	ret = conn_is_name_owner(env->conn, KDBUS_NAME_LIST_NAMES, name);
	ASSERT_RETURN(ret == 0);

	/* ... and release it again */
	ret = kdbus_name_release(env->conn, name);
	ASSERT_RETURN(ret == 0);

	ret = conn_is_name_owner(env->conn, KDBUS_NAME_LIST_NAMES, name);
	ASSERT_RETURN(ret != 0);

	/* check that we can't release it again */
	ret = kdbus_name_release(env->conn, name);
	ASSERT_RETURN(ret == -ESRCH);

	/* check that we can't release a name that we don't own */
	ret = kdbus_name_release(env->conn, "foo.bar.xxx");
	ASSERT_RETURN(ret == -ESRCH);

	return TEST_OK;
}

int kdbus_test_name_conflict(struct kdbus_test_env *env)
{
	struct kdbus_conn *conn;
	char *name;
	int ret;

	name = "foo.bla.blaz";

	/* create a 2nd connection */
	conn = kdbus_hello(env->buspath, 0, NULL, 0);
	ASSERT_RETURN(conn != NULL);

	/* allow the new connection to own the same name */
	/* acquire name from the 1st connection */
	ret = kdbus_name_acquire(env->conn, name, NULL);
	ASSERT_RETURN(ret == 0);

	ret = conn_is_name_owner(env->conn, KDBUS_NAME_LIST_NAMES, name);
	ASSERT_RETURN(ret == 0);

	/* check that we can't acquire it again from the 1st connection */
	ret = kdbus_name_acquire(env->conn, name, NULL);
	ASSERT_RETURN(ret == -EALREADY);

	/* check that we also can't acquire it again from the 2nd connection */
	ret = kdbus_name_acquire(conn, name, NULL);
	ASSERT_RETURN(ret == -EEXIST);

	kdbus_conn_free(conn);

	return TEST_OK;
}

int kdbus_test_name_queue(struct kdbus_test_env *env)
{
	struct kdbus_conn *conn;
	const char *name;
	uint64_t flags;
	int ret;

	name = "foo.bla.blaz";

	flags = KDBUS_NAME_ALLOW_REPLACEMENT;

	/* create a 2nd connection */
	conn = kdbus_hello(env->buspath, 0, NULL, 0);
	ASSERT_RETURN(conn != NULL);

	/* allow the new connection to own the same name */
	/* acquire name from the 1st connection */
	ret = kdbus_name_acquire(env->conn, name, &flags);
	ASSERT_RETURN(ret == 0);

	ret = conn_is_name_owner(env->conn, KDBUS_NAME_LIST_NAMES, name);
	ASSERT_RETURN(ret == 0);

	/* queue the 2nd connection as waiting owner */
	flags = KDBUS_NAME_QUEUE;
	ret = kdbus_name_acquire(conn, name, &flags);
	ASSERT_RETURN(ret == 0);
	ASSERT_RETURN(flags & KDBUS_NAME_IN_QUEUE);

	/* release name from 1st connection */
	ret = kdbus_name_release(env->conn, name);
	ASSERT_RETURN(ret == 0);

	/* now the name should be owned by the 2nd connection */
	ret = conn_is_name_owner(conn, KDBUS_NAME_LIST_NAMES, name);
	ASSERT_RETURN(ret == 0);

	kdbus_conn_free(conn);

	return TEST_OK;
}
