/// <reference path="../pb_data/types.d.ts" />
// Deletes every batch older than 30 days, for every gateway, every hour at minute 17 (TASK-1938, TASK-2903).
// Not configurable on purpose: no stored record lives longer than 30 days plus one hour.
// "created" is stored as "YYYY-MM-DD HH:MM:SS.sssZ", so a string comparison is a time comparison.
cronAdd("otgwBatchRetention", "17 * * * *", () => {
  const cutoff = new Date(Date.now() - 30 * 86400000).toISOString().replace("T", " ");
  const res = $app.db()
    .newQuery("DELETE FROM batches WHERE created < {:cutoff}")
    .bind({ cutoff: cutoff })
    .execute();
  let n = "?";
  try { n = res.rowsAffected(); } catch (e) {}
  console.log("otgw retention: deleted " + n + " batches older than " + cutoff);
});
