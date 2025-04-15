# Notes on the documentation of reply schemata per endpoint
- Always analyze the returned HTTP status code first. Information about success or failure of a request to an endpoint is encoded in the HTTP status code of the reply.
- An endpoint may reply with a message *up to* the specified schema, i.e., the returned message will not contain more fields than indicated here. However, some fields may be left out, or the reply might be empty. Therefore, a client must always check what fields are actually contained, it cannot rely on the message containing all fields in the schema.
	- For example, in a simple success scenario, the reply often will have status code 200 (HTTP OK), and no content.
- There are endpoints for which two different possible schemata are referenced (e.g., the "driver/direction" endpoint can reply with either a message like common_feedback.json or driver_direction.json). In this case, one has to always check which fields are present.



- The above has to be updated after the switch to json-schema (as of 2024-11-04)
- Note: Especially for Admin endpoints, often they don't have any meaningful return instead of "OK" or "Success", which can sufficiently be expressed with the HTTP status code. In an error case, we do want to send some error message though. Therefore, the common-feedback schema they use does not have `required: [msg]`, so that we can omit the message if there's nothing meaningful to say. However, in an error case we really want a message to be there, and a missing error message would currently slip through the json schema validation.