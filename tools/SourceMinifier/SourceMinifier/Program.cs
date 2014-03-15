using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SourceMinifier
{
    class Program
    {
        static void ClipBoradScanner(string swapSetFile)
        {
            // Load dictionary
            var swapSet = new HashSet<string>();
            var lines = File.ReadAllLines(swapSetFile);
            foreach (var line in lines)
                if (!swapSet.Contains(line.Trim()))
                    swapSet.Add(line.Trim());

            // Wait for exit
            Console.WriteLine("Scanning clipboard... (Press any key to exit)");
            Clipboard.Clear();
            while (!Console.KeyAvailable)
            {
                if (Clipboard.ContainsText())
                {
                    // Get text
                    var text = Clipboard.GetText();

                    text = text.Trim();

                    // Check if this is a new word
                    if (!swapSet.Contains(text) && (text != ""))
                    {
                        Console.WriteLine("Adding: " + text);
                        swapSet.Add(text);

                        // Save to file
                        File.WriteAllLines(swapSetFile, swapSet.ToArray());
                    }
                }
            }
        }

        [STAThread]
        static void Main(string[] args)
        {
            Console.WriteLine("SourceMinifier v0.0");

            // Check input
            if (args.Length < 2)
            {
                Console.WriteLine("  Two functionalities:");
                Console.WriteLine("    1. Minify sources  - usage: SourceMinifier <SwapSet.txt> <Source.file> <cpp-output.file>");
                Console.WriteLine("    2. SwapSet builder - usage: SourceMinifier -b <SwapSet.txt>");
                Console.WriteLine("Invalid args");
                return;
            }

            if (args[0].ToLower() == "-b")
            {
                ClipBoradScanner(args[1]);
                return;
            }

            // Load dictionary
            var swapSet = new HashSet<string>();
            var lines = File.ReadAllLines(args[0]);
            foreach (var line in lines)
                if (!swapSet.Contains(line.Trim()))
                    swapSet.Add(line.Trim());

            // Read file
            var text = File.ReadAllText(args[1]);

            var blockComments = @"/\*(.*?)\*/";
            var lineComments = @"//(.*?)\r?\n";
            var strings = @"""((\\[^\n]|[^""\n])*)""";
            var verbatimStrings = @"@(""[^""]*"")+";

            var replacers = new Dictionary<string, string>();
            var comments = new List<String>();

            // Remove comments
            string noComments = Regex.Replace(text, blockComments + "|" + lineComments + "|" + strings + "|" + verbatimStrings,
                me =>
                {
                    if (me.Value.StartsWith("/*") || me.Value.StartsWith("//"))
                    {
                        comments.Add(me.Value);
                        return me.Value.StartsWith("//") ? Environment.NewLine : "";
                    }                   

                    // Keep the literal strings
                    return me.Value;
                },
                RegexOptions.Singleline);

            // Build dictionary
            int index = 1;
            foreach (var word in swapSet)
                replacers.Add(word, "p" + index++);

            // Replace things
            string prevItem = "";
            string output = Regex.Replace(noComments, @"\.?([a-zA-Z0-9_]+)", me =>
                {
                    if (replacers.ContainsKey(me.Value))
                        prevItem = replacers[me.Value];
                    else
                        prevItem = me.Value;

                    return prevItem;
                }, RegexOptions.Singleline);

            // Process lines
            var sb = new StringBuilder();
            foreach (var line in output.Split(new[] { '\r', '\n' }))
            {
                // Trim line
                var newLine = line.Trim();

                // Remove double spaces
                RegexOptions options = RegexOptions.None;
                Regex regex = new Regex(@"[ ]{2,}", options);
                newLine = regex.Replace(newLine, @" ");

                // Add line to ouptut
                if (!string.IsNullOrEmpty(newLine))
                {
                    if (newLine.Length > 1)
                        sb.AppendLine("");

                    sb.Append(newLine);
                }
            }
            output = sb.ToString();

            var OutPath = Path.Combine(Path.GetDirectoryName(args[1]), "Obs");
            Directory.CreateDirectory(OutPath);
            File.WriteAllText(Path.Combine(OutPath, Path.GetFileName(args[1])), output);

            // Build C++ output
            var sb_cpp = new StringBuilder();
            sb_cpp.AppendLine(@"g_code_resources[""" + Path.GetFileName(args[1]) + @"""] = """"");
            foreach (var line in output.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries))
                sb_cpp.AppendLine(@"    """ + line.Replace(@"""", @"\""") + @"\n""");
            sb_cpp.AppendLine(";");
            sb_cpp.AppendLine("");

            File.AppendAllText(args[2], sb_cpp.ToString());
        }
    }
}
