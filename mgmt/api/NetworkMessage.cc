/** @file

  Network message marshalling.

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#include "ts/ink_config.h"
#include "ts/ink_defs.h"
#include "ts/ink_assert.h"
#include "ts/ink_memory.h"
#include "mgmtapi.h"
#include "NetworkMessage.h"

#define MAX_OPERATION_BUFSZ 1024
#define MAX_OPERATION_FIELDS 16

struct NetCmdOperation {
  unsigned nfields;
  const MgmtMarshallType fields[MAX_OPERATION_FIELDS];
};

// Requests always begin with a OpType, followed by aditional fields.
static const struct NetCmdOperation requests[] = {
  /* FILE_READ                  */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT}},
  /* FILE_WRITE                 */ {4, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT, MGMT_MARSHALL_INT, MGMT_MARSHALL_DATA}},
  /* RECORD_SET                 */ {3, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING, MGMT_MARSHALL_STRING}},
  /* RECORD_GET                 */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* PROXY_STATE_GET            */ {1, {MGMT_MARSHALL_INT}},
  /* PROXY_STATE_SET            */ {3, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT, MGMT_MARSHALL_INT}},
  /* RECONFIGURE                */ {1, {MGMT_MARSHALL_INT}},
  /* RESTART                    */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT}},
  /* BOUNCE                     */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT}},
  /* EVENT_RESOLVE              */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* EVENT_GET_MLT              */ {1, {MGMT_MARSHALL_INT}},
  /* EVENT_ACTIVE               */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* EVENT_REG_CALLBACK         */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* EVENT_UNREG_CALLBACK       */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* EVENT_NOTIFY               */ {3,
                                    {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING,
                                     MGMT_MARSHALL_STRING}}, // only msg sent from TM to client
  /* SNAPSHOT_TAKE              */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* SNAPSHOT_RESTORE           */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* SNAPSHOT_REMOVE            */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* SNAPSHOT_GET_MLT           */ {1, {MGMT_MARSHALL_INT}},
  /* DIAGS                      */ {3, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* STATS_RESET_NODE           */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* STATS_RESET_CLUSTER        */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* STORAGE_DEVICE_CMD_OFFLINE */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* RECORD_MATCH_GET           */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* API_PING                   */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT}},
  /* SERVER_BACKTRACE           */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT}},
  /* RECORD_DESCRIBE_CONFIG     */ {3, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING, MGMT_MARSHALL_INT}},
  /* LIFECYCLE_MESSAGE          */ {3, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING, MGMT_MARSHALL_DATA}},
};

// Responses always begin with a TSMgmtError code, followed by additional fields.
static const struct NetCmdOperation responses[] = {
  /* FILE_READ                  */ {3, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT, MGMT_MARSHALL_DATA}},
  /* FILE_WRITE                 */ {1, {MGMT_MARSHALL_INT}},
  /* RECORD_SET                 */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT}},
  /* RECORD_GET                 */ {5,
                                    {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT, MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING,
                                     MGMT_MARSHALL_DATA}},
  /* PROXY_STATE_GET            */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT}},
  /* PROXY_STATE_SET            */ {1, {MGMT_MARSHALL_INT}},
  /* RECONFIGURE                */ {1, {MGMT_MARSHALL_INT}},
  /* RESTART                    */ {1, {MGMT_MARSHALL_INT}},
  /* BOUNCE                     */ {1, {MGMT_MARSHALL_INT}},
  /* EVENT_RESOLVE              */ {1, {MGMT_MARSHALL_INT}},
  /* EVENT_GET_MLT              */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* EVENT_ACTIVE               */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT}},
  /* EVENT_REG_CALLBACK         */ {0, {}}, // no reply
  /* EVENT_UNREG_CALLBACK       */ {0, {}}, // no reply
  /* EVENT_NOTIFY               */ {0, {}}, // no reply
  /* SNAPSHOT_TAKE              */ {1, {MGMT_MARSHALL_INT}},
  /* SNAPSHOT_RESTORE           */ {1, {MGMT_MARSHALL_INT}},
  /* SNAPSHOT_REMOVE            */ {1, {MGMT_MARSHALL_INT}},
  /* SNAPSHOT_GET_MLT           */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* DIAGS                      */ {0, {}}, // no reply
  /* STATS_RESET_NODE           */ {1, {MGMT_MARSHALL_INT}},
  /* STATS_RESET_CLUSTER        */ {1, {MGMT_MARSHALL_INT}},
  /* STORAGE_DEVICE_CMD_OFFLINE */ {1, {MGMT_MARSHALL_INT}},
  /* RECORD_MATCH_GET           */ {5,
                                    {MGMT_MARSHALL_INT, MGMT_MARSHALL_INT, MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING,
                                     MGMT_MARSHALL_DATA}},
  /* API_PING                   */ {0, {}}, // no reply
  /* SERVER_BACKTRACE           */ {2, {MGMT_MARSHALL_INT, MGMT_MARSHALL_STRING}},
  /* RECORD_DESCRIBE_CONFIG     */ {15,
                                    {MGMT_MARSHALL_INT /* status */, MGMT_MARSHALL_STRING /* name */,
                                     MGMT_MARSHALL_DATA /* value */, MGMT_MARSHALL_DATA /* default */, MGMT_MARSHALL_INT /* type */,
                                     MGMT_MARSHALL_INT /* class */, MGMT_MARSHALL_INT /* version */, MGMT_MARSHALL_INT /* rsb */,
                                     MGMT_MARSHALL_INT /* order */, MGMT_MARSHALL_INT /* access */, MGMT_MARSHALL_INT /* update */,
                                     MGMT_MARSHALL_INT /* updatetype */, MGMT_MARSHALL_INT /* checktype */,
                                     MGMT_MARSHALL_INT /* source */, MGMT_MARSHALL_STRING /* checkexpr */}},
  /* LIFECYCLE_MESSAGE          */ {1, {MGMT_MARSHALL_INT}},
};

#define GETCMD(ops, optype, cmd)                                       \
  do {                                                                 \
    if (optype < 0 || static_cast<unsigned>(optype) >= countof(ops)) { \
      return TS_ERR_PARAMS;                                            \
    }                                                                  \
    if (ops[optype].nfields == 0) {                                    \
      return TS_ERR_PARAMS;                                            \
    }                                                                  \
    cmd = &ops[optype];                                                \
  } while (0);

TSMgmtError
send_mgmt_request(const mgmt_message_sender &snd, OpType optype, ...)
{
  va_list ap;
  ats_scoped_mem<char> msgbuf;
  MgmtMarshallInt msglen;
  const MgmtMarshallType lenfield[] = {MGMT_MARSHALL_INT};
  const NetCmdOperation *cmd;

  GETCMD(requests, optype, cmd);

  va_start(ap, optype);
  msglen = mgmt_message_length_v(cmd->fields, cmd->nfields, ap);
  va_end(ap);

  msgbuf = (char *)ats_malloc(msglen + 4);

  // First marshall the total message length.
  mgmt_message_marshall((char *)msgbuf, msglen, lenfield, countof(lenfield), &msglen);

  // Now marshall the message itself.
  va_start(ap, optype);
  if (mgmt_message_marshall_v((char *)msgbuf + 4, msglen, cmd->fields, cmd->nfields, ap) == -1) {
    va_end(ap);
    return TS_ERR_PARAMS;
  }

  va_end(ap);
  return snd.send(msgbuf, msglen + 4);
}

TSMgmtError
send_mgmt_request(int fd, OpType optype, ...)
{
  va_list ap;
  MgmtMarshallInt msglen;
  MgmtMarshallData req = {NULL, 0};
  const MgmtMarshallType fields[] = {MGMT_MARSHALL_DATA};
  const NetCmdOperation *cmd;

  GETCMD(requests, optype, cmd);

  // Figure out the payload length.
  va_start(ap, optype);
  msglen = mgmt_message_length_v(cmd->fields, cmd->nfields, ap);
  va_end(ap);

  ink_assert(msglen >= 0);

  req.ptr = (char *)ats_malloc(msglen);
  req.len = msglen;

  // Marshall the message itself.
  va_start(ap, optype);
  if (mgmt_message_marshall_v(req.ptr, req.len, cmd->fields, cmd->nfields, ap) == -1) {
    ats_free(req.ptr);
    va_end(ap);
    return TS_ERR_PARAMS;
  }

  va_end(ap);

  // Send the response as the payload of a data object.
  if (mgmt_message_write(fd, fields, countof(fields), &req) == -1) {
    ats_free(req.ptr);
    return TS_ERR_NET_WRITE;
  }

  ats_free(req.ptr);
  return TS_ERR_OKAY;
}

TSMgmtError
send_mgmt_error(int fd, OpType optype, TSMgmtError error)
{
  MgmtMarshallInt ecode = error;
  MgmtMarshallInt intval = 0;
  MgmtMarshallData dataval = {NULL, 0};
  MgmtMarshallString strval = NULL;

  // Switch on operations, grouped by response format.
  switch (optype) {
  case FILE_WRITE:
  case PROXY_STATE_SET:
  case RECONFIGURE:
  case RESTART:
  case BOUNCE:
  case EVENT_RESOLVE:
  case SNAPSHOT_TAKE:
  case SNAPSHOT_RESTORE:
  case SNAPSHOT_REMOVE:
  case STATS_RESET_NODE:
  case STATS_RESET_CLUSTER:
  case STORAGE_DEVICE_CMD_OFFLINE:
    ink_release_assert(responses[optype].nfields == 1);
    return send_mgmt_response(fd, optype, &ecode);

  case RECORD_SET:
  case PROXY_STATE_GET:
  case EVENT_ACTIVE:
    ink_release_assert(responses[optype].nfields == 2);
    return send_mgmt_response(fd, optype, &ecode, &intval);

  case EVENT_GET_MLT:
  case SNAPSHOT_GET_MLT:
  case SERVER_BACKTRACE:
    ink_release_assert(responses[optype].nfields == 2);
    return send_mgmt_response(fd, optype, &ecode, &strval);

  case FILE_READ:
  case LIFECYCLE_MESSAGE:
    ink_release_assert(responses[optype].nfields == 3);
    return send_mgmt_response(fd, optype, &ecode, &intval, &dataval);

  case RECORD_GET:
  case RECORD_MATCH_GET:
    ink_release_assert(responses[optype].nfields == 5);
    return send_mgmt_response(fd, optype, &ecode, &intval, &intval, &strval, &dataval);

  case RECORD_DESCRIBE_CONFIG:
    ink_release_assert(responses[optype].nfields == 15);
    return send_mgmt_response(fd, optype, &ecode, &strval /* name */, &dataval /* value */, &dataval /* default */,
                              &intval /* type */, &intval /* class */, &intval /* version */, &intval /* rsb */,
                              &intval /* order */, &intval /* access */, &intval /* update */, &intval /* updatetype */,
                              &intval /* checktype */, &intval /* source */, &strval /* checkexpr */);

  case EVENT_REG_CALLBACK:
  case EVENT_UNREG_CALLBACK:
  case EVENT_NOTIFY:
  case DIAGS:
  case API_PING:
    /* no response for these */
    ink_release_assert(responses[optype].nfields == 0);
    return TS_ERR_OKAY;

  case UNDEFINED_OP:
    return TS_ERR_OKAY;
  }

  // We should never get here unless OpTypes are added without
  // updating the switch statement above. Don't do that; this
  // code must be able to handle every OpType.

  ink_fatal("missing generic error support for type %d management message", optype);
  return TS_ERR_FAIL;
}

// Send a management message response. We don't need to worry about retransmitting the message if we get
// disconnected, so this is much simpler. We can directly marshall the response as a data object.
TSMgmtError
send_mgmt_response(int fd, OpType optype, ...)
{
  va_list ap;
  MgmtMarshallInt msglen;
  MgmtMarshallData reply = {NULL, 0};
  const MgmtMarshallType fields[] = {MGMT_MARSHALL_DATA};
  const NetCmdOperation *cmd;

  GETCMD(responses, optype, cmd);

  va_start(ap, optype);
  msglen = mgmt_message_length_v(cmd->fields, cmd->nfields, ap);
  va_end(ap);

  ink_assert(msglen >= 0);

  reply.ptr = (char *)ats_malloc(msglen);
  reply.len = msglen;

  // Marshall the message itself.
  va_start(ap, optype);
  if (mgmt_message_marshall_v(reply.ptr, reply.len, cmd->fields, cmd->nfields, ap) == -1) {
    ats_free(reply.ptr);
    va_end(ap);
    return TS_ERR_PARAMS;
  }

  va_end(ap);

  // Send the response as the payload of a data object.
  if (mgmt_message_write(fd, fields, countof(fields), &reply) == -1) {
    ats_free(reply.ptr);
    return TS_ERR_NET_WRITE;
  }

  ats_free(reply.ptr);
  return TS_ERR_OKAY;
}

template <unsigned N>
static TSMgmtError
recv_x(const struct NetCmdOperation (&ops)[N], void *buf, size_t buflen, OpType optype, va_list ap)
{
  ssize_t msglen;
  const NetCmdOperation *cmd;

  GETCMD(ops, optype, cmd);

  msglen = mgmt_message_parse_v(buf, buflen, cmd->fields, cmd->nfields, ap);
  return (msglen == -1) ? TS_ERR_PARAMS : TS_ERR_OKAY;
}

TSMgmtError
recv_mgmt_request(void *buf, size_t buflen, OpType optype, ...)
{
  TSMgmtError err;
  va_list ap;

  va_start(ap, optype);
  err = recv_x(requests, buf, buflen, optype, ap);
  va_end(ap);

  return err;
}

TSMgmtError
recv_mgmt_response(void *buf, size_t buflen, OpType optype, ...)
{
  TSMgmtError err;
  va_list ap;

  va_start(ap, optype);
  err = recv_x(responses, buf, buflen, optype, ap);
  va_end(ap);

  return err;
}

TSMgmtError
recv_mgmt_message(int fd, MgmtMarshallData &msg)
{
  const MgmtMarshallType fields[] = {MGMT_MARSHALL_DATA};

  if (mgmt_message_read(fd, fields, countof(fields), &msg) == -1) {
    return TS_ERR_NET_READ;
  }

  return TS_ERR_OKAY;
}

OpType
extract_mgmt_request_optype(void *msg, size_t msglen)
{
  const MgmtMarshallType fields[] = {MGMT_MARSHALL_INT};
  MgmtMarshallInt optype;

  if (mgmt_message_parse(msg, msglen, fields, countof(fields), &optype) == -1) {
    return UNDEFINED_OP;
  }

  return (OpType)optype;
}
