
/**
 * @brief The task used to demonstrate the FP API.
 *
 * This task uses the provided claim key and certificate files to connect to
 * AWS and use PKCS #11 to generate a new device key and certificate with a CSR.
 * The task then creates a new Thing with the Fleet Provisioning API using the
 * newly-created credentials. The task finishes by connecting to the newly-created
 * Thing to verify that it was successfully created and accessible using the key/cert.
 *
 * @param[in] pvParameters Parameters as passed at the time of task creation.
 * Not used in this example.
 */
int prvFleetProvisioningTask( void * pvParameters );
