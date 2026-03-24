#!/bin/bash

PROFILE=${1:-default}
REGION=${2:-us-west-1}
DELAY_INTERVAL=10
DELAY_DURATION=120  # seconds

echo "🔍 Listing OTA jobs using profile: $PROFILE (region: $REGION)..."

START_TIME=$(date +%s)

JOB_IDS=$(aws iot list-jobs --profile "$PROFILE" --region "$REGION" --query "jobs[].jobId" --output text)

if [ -z "$JOB_IDS" ]; then
    echo "✅ No jobs found."
    exit 0
fi

echo "🧹 Deleting jobs..."

COUNT=0
for JOB_ID in $JOB_IDS; do
    JOB_START=$(date +%s)

    echo "→ Deleting job: $JOB_ID"
    aws iot delete-job \
        --job-id "$JOB_ID" \
        --profile "$PROFILE" \
        --region "$REGION" \
        --force

    ((COUNT++))
    JOB_END=$(date +%s)
    JOB_DURATION=$((JOB_END - JOB_START))
    echo "⏱ Deleted in ${JOB_DURATION}s | Total deleted: $COUNT"

    if (( COUNT % DELAY_INTERVAL == 0 )); then
        echo "⏸ Pausing for 2 minutes to avoid throttling..."
        sleep "$DELAY_DURATION"
    fi
done

END_TIME=$(date +%s)
TOTAL_DURATION=$((END_TIME - START_TIME))

echo "🎉 All jobs deleted. Total: $COUNT"
echo "🕒 Script execution time: ${TOTAL_DURATION}s"
