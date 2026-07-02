const cron = require('node-cron');
const { db, statements } = require('./db');

function startCronJobs() {
  // Run every night at 03:00 (Data Retention Policy)
  cron.schedule('0 3 * * *', () => {
    console.log('[Cron] Starting nightly database maintenance & cleanup...');
    try {
      const thirtyDaysAgo = Date.now() - (30 * 24 * 60 * 60 * 1000);
      
      // Delete old telemetry
      const startDelete = Date.now();
      const deleteResult = statements.deleteOldTelemetry.run(thirtyDaysAgo);
      const deleteTime = Date.now() - startDelete;
      console.log(`[Cron] Purged ${deleteResult.changes} telemetry records older than 30 days in ${deleteTime}ms.`);
      
      // SQLite optimize to analyze query statistics and compile better plans
      const startOptimize = Date.now();
      db.exec('PRAGMA optimize;');
      const optimizeTime = Date.now() - startOptimize;
      console.log(`[Cron] Database optimization (PRAGMA optimize) completed in ${optimizeTime}ms.`);
      
    } catch (err) {
      console.error('[Cron] Error running database maintenance job:', err);
    }
  });
  
  console.log('[Cron] Nightly maintenance job scheduled at 03:00.');
}

module.exports = {
  startCronJobs
};
