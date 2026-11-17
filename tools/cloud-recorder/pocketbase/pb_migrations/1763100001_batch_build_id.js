/// <reference path="../pb_data/types.d.ts" />

// Optional for batches written by gateways predating per-batch build IDs.
migrate((app) => {
  const batches = app.findCollectionByNameOrId("batches");
  batches.fields.add(new TextField({ name: "build_id", min: 64, max: 64, pattern: "^[a-f0-9]{64}$" }));
  app.save(batches);
}, (app) => {
  const batches = app.findCollectionByNameOrId("batches");
  batches.fields.removeByName("build_id");
  app.save(batches);
});
