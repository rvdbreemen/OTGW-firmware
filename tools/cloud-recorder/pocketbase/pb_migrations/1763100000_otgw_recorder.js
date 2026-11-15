/// <reference path="../pb_data/types.d.ts" />
// OTGW diagnostic collection point (TASK-1938, TASK-1971). All logic is in collection rules; the only code is
// the retention cron in pb_hooks/retention.pb.js.
//   devices: the register of gateways that log, one auth record each. A gateway enrolls itself on first contact
//            with its unique id (uid, from the MAC) and a secret it generated; it logs in with uid + secret.
//            "blocked" is set by the maintainer in the dashboard; a gateway can neither set nor clear it.
//   batches: create-only for the device that owns the batch, and only while it is not blocked. Nobody but a
//            superuser can list, read, change or delete batches.
migrate((app) => {
  const devices = new Collection({
    type: "auth",
    name: "devices",
    listRule: null,
    viewRule: null,
    // Anyone may enroll a new uid (the unique index refuses an existing one); "blocked" may not be sent.
    createRule: '@request.body.blocked:isset = false && @request.body.verified:isset = false',
    // A gateway may update its own label, firmware and board; not its uid and not "blocked".
    updateRule: 'id = @request.auth.id && @request.body.blocked:isset = false && @request.body.uid:isset = false' +
                ' && @request.body.verified:isset = false',
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
    createRule: '@request.auth.collectionName = "devices" && @request.auth.blocked = false' +
                ' && @request.body.device = @request.auth.id',
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
