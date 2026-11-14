/// <reference path="../pb_data/types.d.ts" />
// OTGW diagnostic collection point (TASK-1938).
//   devices: one auth record per gateway; only enabled devices can log in.
//   batches: create-only for the device that owns the batch; nobody but a
//            superuser can list, read, change or delete batches.
migrate((app) => {
  const devices = new Collection({
    type: "auth",
    name: "devices",
    listRule: null,
    viewRule: null,
    createRule: null,
    updateRule: null,
    deleteRule: null,
    authRule: "enabled = true",
    passwordAuth: { enabled: true, identityFields: ["email"] },
    authToken: { duration: 1209600 },   // 14 days; the device refreshes before expiry
    fields: [
      { name: "label", type: "text", max: 80 },
      { name: "enabled", type: "bool" },
    ],
  });
  app.save(devices);

  const batches = new Collection({
    type: "base",
    name: "batches",
    listRule: null,
    viewRule: null,
    updateRule: null,
    deleteRule: null,
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

  // Real client IP behind the tunnel, and rate limits per IP.
  const settings = app.settings();
  settings.trustedProxy.headers = ["CF-Connecting-IP"];
  settings.rateLimits.enabled = true;
  settings.rateLimits.rules = [
    { label: "batches:create", maxRequests: 30, duration: 60 },
    { label: "/api/collections/devices/auth-with-password", maxRequests: 5, duration: 60 },
    { label: "/api/", maxRequests: 120, duration: 60 },
  ];
  app.save(settings);
}, (app) => {
  app.delete(app.findCollectionByNameOrId("batches"));
  app.delete(app.findCollectionByNameOrId("devices"));
});
