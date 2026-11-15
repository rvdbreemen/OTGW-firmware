/// <reference path="../pb_data/types.d.ts" />
// OTGW diagnostic collection point (TASK-1938, TASK-1971, TASK-1972). The logic is in collection rules plus two
// small hooks: the retention cron (pb_hooks/retention.pb.js) and the blocked reply (pb_hooks/blocked.pb.js).
//   devices: the register of gateways that log, one auth record each. A gateway enrolls itself on first contact
//            with its unique id (uid, from the MAC) and a secret it generated; it logs in with uid + secret.
//            "blocked" is set by the maintainer in the dashboard; a gateway can neither set nor clear it.
//   batches: create-only for the device that owns the batch (a blocked one gets 403, pb_hooks/blocked.pb.js). Nobody but a
//            superuser can list, read, change or delete batches.
migrate((app) => {
  const OTGW_MODE_FIELDS_UNSET = ' && @request.body.mode:isset = false && @request.body.flags:isset = false' +
                                 ' && @request.body.mode_until:isset = false && @request.body.mode_note:isset = false';
  const devices = new Collection({
    type: "auth",
    name: "devices",
    listRule: null,
    viewRule: null,
    // Anyone may enroll a new uid (the unique index refuses an existing one); "blocked" may not be sent.
    // The maintainer's fields (blocked, and the telemetry mode of TASK-1979) may not be sent by a gateway.
    createRule: '@request.body.blocked:isset = false && @request.body.verified:isset = false' + OTGW_MODE_FIELDS_UNSET,
    // A gateway may update its own label, firmware and board; not its uid and not "blocked".
    updateRule: 'id = @request.auth.id && @request.body.blocked:isset = false && @request.body.uid:isset = false' +
                ' && @request.body.verified:isset = false' + OTGW_MODE_FIELDS_UNSET,
    deleteRule: null,
    authRule: "",                       // a blocked gateway can still log in: it reads "blocked" and turns itself off
    passwordAuth: { enabled: true, identityFields: ["uid"] },
    authToken: { duration: 1209600 },   // 14 days
    fields: [
      { name: "uid", type: "text", required: true, pattern: "^otgw-[0-9a-f]{12}$", min: 17, max: 17 },
      { name: "label", type: "text", max: 80 },
      { name: "fw", type: "text", max: 64 },
      { name: "board", type: "text", max: 32 },
      { name: "blocked", type: "bool" },
      // TASK-1979: telemetry mode, set by the maintainer; empty = default. The gateway reads it at every login and
      // token refresh (every 15 min) and goes back to default by itself after mode_until.
      { name: "mode", type: "select", values: ["default", "full", "full+trace", "custom"], maxSelect: 1 },
      { name: "flags", type: "select", maxSelect: 10,
        values: ["otmsg", "restapi", "mqtt", "sensors", "sat", "otdirect", "satble", "sattrace", "mqttgate", "ntp"] },
      { name: "mode_until", type: "date" },
      { name: "mode_note", type: "text", max: 200 },
      { name: "updated", type: "autodate", onCreate: true, onUpdate: true },   // last login
    ],
    indexes: ["CREATE UNIQUE INDEX idx_devices_uid ON devices (uid)"],
  });
  app.save(devices);
  devices.fields.getByName("email").required = false;   // a gateway has no mailbox; it logs in with its uid
  app.save(devices);

  const batches = new Collection({
    type: "base",
    name: "batches",
    listRule: null,
    viewRule: null,
    updateRule: null,
    deleteRule: null,
    // A blocked gateway is refused by pb_hooks/blocked.pb.js with 403 "otgw-blocked" (the rule would give a bare 400).
    createRule: '@request.auth.collectionName = "devices" && @request.body.device = @request.auth.id',
    fields: [
      { name: "device", type: "relation", collectionId: devices.id, maxSelect: 1, required: true, cascadeDelete: true },
      { name: "stream", type: "select", values: ["ot", "sat", "dbg", "mix"], maxSelect: 1, required: true },
      { name: "seq", type: "number", onlyInt: true, min: 0 },            // per device and stream, counts up
      { name: "t0", type: "number", onlyInt: true, required: true },     // epoch ms of the first record
      { name: "fmt", type: "number", onlyInt: true, min: 1, max: 255, required: true },
      // base64 of the encoded chunk; 65536 chars = 48 KiB binary, room for 60 s of full debug output
      // in the token+zlib form (largest measured batch 21.7 KB, dbg_size_study.py)
      { name: "payload", type: "text", max: 65536, required: true },
      { name: "created", type: "autodate", onCreate: true, onUpdate: false },
    ],
    indexes: [
      "CREATE UNIQUE INDEX idx_batches_dev_stream_seq ON batches (device, stream, seq)",
      "CREATE INDEX idx_batches_created ON batches (created)",
    ],
  });
  app.save(batches);

  // Real client IP behind the tunnel, rate limits per IP, and request logs (they hold IP addresses) for 7 days.
  const settings = app.settings();
  settings.trustedProxy.headers = ["CF-Connecting-IP"];
  settings.logs.maxDays = 7;
  settings.rateLimits.enabled = true;
  settings.rateLimits.rules = [
    { label: "batches:create", maxRequests: 30, duration: 60 },
    { label: "devices:create", maxRequests: 3, duration: 60 },
    { label: "/api/collections/devices/auth-with-password", maxRequests: 5, duration: 60 },
    { label: "/api/", maxRequests: 120, duration: 60 },
  ];
  app.save(settings);
}, (app) => {
  app.delete(app.findCollectionByNameOrId("batches"));
  app.delete(app.findCollectionByNameOrId("devices"));
});
