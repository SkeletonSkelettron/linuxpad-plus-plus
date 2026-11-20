// SPDX-License-Identifier: Apache-2.0
// Local stdin/stdout adapter for the JsonTools parser and query engines.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Text.Json;
using JSON_Tools.JSON_Tools;

internal sealed record ToolRequest(string Operation, string Text, string Argument = "",
    bool JsonLines = false, int Position = 0, int Indent = 4);

internal sealed record TreeNode(string Key, string Value, int Position, List<TreeNode> Children);

internal sealed record ToolResponse(bool Ok, string Text = "", string Error = "", int Position = -1,
    TreeNode Tree = null, bool Truncated = false);

internal static class Program
{
    private static readonly JsonSerializerOptions Wire = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        MaxDepth = 256
    };

    private static void Main()
    {
        CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
        CultureInfo.CurrentUICulture = CultureInfo.InvariantCulture;
        ToolResponse response;
        try
        {
            ToolRequest request = JsonSerializer.Deserialize<ToolRequest>(Console.OpenStandardInput(), Wire);
            response = request is null || request.Text is null
                ? new(false, Error: "Missing JSON input.")
                : Run(request);
            if (Encoding.UTF8.GetByteCount(response.Text) > 16 * 1024 * 1024)
                response = new(false, Error: "The result exceeds 16 MiB.");
        }
        catch (JsonParserException ex)
        {
            response = new(false, Error: ex.Translate(false), Position: ex.Position);
        }
        catch (Exception ex)
        {
            response = new(false, Error: ex.Message);
        }
        JsonSerializer.Serialize(Console.OpenStandardOutput(), response, Wire);
    }

    private static ToolResponse Run(ToolRequest request)
    {
        if (request.Operation == "quote")
            return new(true, new JNode(request.Text).ToString());

        var parser = new JsonParser(request.Operation == "check" ? LoggerLevel.STRICT : LoggerLevel.JSON5,
            throwIfLogged: true, throwIfFatal: true, rememberComments: request.Operation == "pretty");
        JNode document = request.JsonLines ? parser.ParseJsonLines(request.Text) : parser.Parse(request.Text);
        int indent = Math.Clamp(request.Indent, 1, 8);

        switch (request.Operation)
        {
            case "check":
                return new(true, "Valid JSON");
            case "pretty":
                return new(true, parser.comments != null && parser.comments.Count > 0
                    ? document.PrettyPrintWithComments(parser.comments, indent, sortKeys: false)
                    : document.PrettyPrint(indent, sortKeys: false));
            case "compact":
                return new(true, Compact(document));
            case "path":
                return new(true, document.PathToPosition(request.Position));
            case "tree":
            case "query":
            {
                bool query = request.Operation == "query";
                JNode result = query ? new RemesParser().Search(request.Argument, document) : document;
                int remaining = 5000;
                TreeNode tree = BuildTree(result, "$", query, 0, ref remaining);
                return new(true, query ? result.PrettyPrint(indent, sortKeys: false) : "",
                    Tree: tree, Truncated: remaining <= 0);
            }
            case "validate":
            {
                JNode schema = new JsonParser(LoggerLevel.JSON5).Parse(request.Argument);
                bool valid = JsonSchemaValidator.Validates(document, schema, 12, false, out List<JsonLint> lints);
                return valid ? new(true, "JSON matches the schema.") : new(false,
                    Error: string.Join("\n", lints.Select(lint => lint.message)),
                    Position: lints.Count > 0 ? lints[0].pos : -1);
            }
            case "schema":
                return new(true, JsonSchemaMaker.GetSchema(document).PrettyPrint(indent, sortKeys: false));
            case "random":
                return new(true, RandomJsonFromSchema.RandomJson(document, 1, 6, false, true).PrettyPrint(indent, sortKeys: false));
            case "yaml":
                return new(true, new YamlDumper().Dump(document));
            case "jsonl":
                return document is JArray array
                    ? new(true, string.Join("\n", array.children.Select(Compact)))
                    : new(false, Error: "Select a JSON array to convert to JSON Lines.");
            case "sort":
                if (document is not JArray sortable)
                    return new(false, Error: "Select a JSON array to sort.");
                sortable.children.Sort();
                return new(true, sortable.PrettyPrint(indent, sortKeys: false));
            case "unquote":
                return document.type == Dtype.STR
                    ? new(true, (string)document.value)
                    : new(false, Error: "Select a JSON string to unquote.");
            default:
                return new(false, Error: "Unknown JSONTools operation.");
        }
    }

    private static string Compact(JNode value) => value.ToString(sortKeys: false, keyValueSep: ":", itemSep: ",");

    private static TreeNode BuildTree(JNode value, string key, bool query, int depth, ref int remaining)
    {
        if (depth > 90)
        {
            remaining = 0;
            return null;
        }
        --remaining;
        var children = new List<TreeNode>();
        string description;
        if (value is JObject obj)
        {
            description = $"Object ({obj.Length})";
            foreach (var member in obj.children)
            {
                if (remaining <= 0)
                    break;
                TreeNode child = BuildTree(member.Value, member.Key, query, depth + 1, ref remaining);
                if (child is not null)
                    children.Add(child);
            }
        }
        else if (value is JArray array)
        {
            description = $"Array ({array.Length})";
            for (int i = 0; i < array.Length && remaining > 0; ++i)
            {
                TreeNode child = BuildTree(array.children[i], $"[{i}]", query, depth + 1, ref remaining);
                if (child is not null)
                    children.Add(child);
            }
        }
        else
        {
            description = value.ToString();
            if (description.Length > 300)
                description = description[..300] + "…";
        }
        return new(key, description, query ? -1 : value.position, children);
    }
}
