/// <reference path="../pb_data/types.d.ts" />
// TASK-2904: every request of a blocked gateway gets HTTP 403 with a message that holds "otgw-blocked", so the
// gateway knows at once that it must turn its telemetry opt-in off and stop connecting. Refused: login, token
// refresh, an update of its own record, and batch uploads (also with a token from before the block).
// (Handlers run in their own context, so the message is written out in each.)

onRecordAuthWithPasswordRequest((e) => {
  if (e.record && e.record.getBool("blocked")) throw new ForbiddenError("Gateway blocked (otgw-blocked): telemetry opt-in off");
  e.next();
}, "devices");

onRecordAuthRequest((e) => {
  if (e.record.getBool("blocked")) throw new ForbiddenError("Gateway blocked (otgw-blocked): telemetry opt-in off");
  e.next();
}, "devices");

onRecordUpdateRequest((e) => {
  const a = e.auth;
  if (a && a.collection().name === "devices" && a.getBool("blocked")) throw new ForbiddenError("Gateway blocked (otgw-blocked): telemetry opt-in off");
  e.next();
}, "devices");

onRecordCreateRequest((e) => {
  const a = e.auth;
  if (a && a.collection().name === "devices" && a.getBool("blocked")) throw new ForbiddenError("Gateway blocked (otgw-blocked): telemetry opt-in off");
  e.next();
}, "batches");
