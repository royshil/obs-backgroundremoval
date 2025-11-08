import { getLatestRelease } from "../../lib/github";

export async function GET() {
  const release = await getLatestRelease();
  const tagName = release.tag_name;
  return new Response(tagName, {
    status: 200,
    headers: {
      "Content-Type": "text/plain",
    },
  });
}
