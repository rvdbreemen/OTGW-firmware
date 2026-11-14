/// <reference path="../pb_data/types.d.ts" />
// Deletes batches older than RETENTION_DAYS (default 30), every night at 03:17 UTC (TASK-1938).
// "created" is stored as "YYYY-MM-DD HH:MM:SS.sssZ", so a string comparison is a time comparison.
cronAdd("otgwBatchRetention", "17 3 * * *", () => {
  let days = parseInt($os.getenv("RETENTION_DAYS"), 10);
  if (isNaN(days) || days < 0) days = 30;
  const cutoff = new Date(Date.now() - days * 86400000).toISOString().replace("T", " ");
  const res = $app.db()
    .newQuery("DELETE FROM batches WHERE created < {:cutoff}")
    .bind({ cutoff: cutoff })
    .execute();
  let n = "?";
  try { n = res.rowsAffected(); } catch (e) {}
  console.log("otgw retention: deleted " + n + " batches older than " + cutoff);
});
